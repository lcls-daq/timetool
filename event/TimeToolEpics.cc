#include "TimeToolEpics.hh"

#include "timetool/service/ConfigCache.hh"
#include "pds/epicstools/PVWriter.hh"
#include "pds/config/TimeToolConfigType.hh"
#include "pds/config/TimeToolDataType.hh"

#include "cadef.h"

#include <string>

using namespace Pds;
using Pds_Epics::PVWriter;
using ::TimeTool::ConfigCache;

static bool _initialized=false;

Pds_TimeTool_event::TimeToolEpics::TimeToolEpics(const char* prefix) :
  _pvts(0.0),
  _prefix(prefix)
{}

Pds_TimeTool_event::TimeToolEpics::~TimeToolEpics() {}

Transition* Pds_TimeTool_event::TimeToolEpics::transitions(Transition* tr)
{
  switch (tr->id()) {
  case TransitionId::Configure:
    if (!_initialized) {
      //  EPICS thread initialization
      SEVCHK ( ca_context_create(ca_enable_preemptive_callback ), 
               "Calling ca_context_create" );
      _initialized=true;
    } break;
  case TransitionId::Unconfigure:
    {
      for(std::map<uint32_t,ConfigCache*>::iterator it=_cfgs.begin();
          it!=_cfgs.end(); it++)
        delete it->second;
      _cfgs.clear();
      for(std::map<uint32_t,PVWriter*>::iterator it=_pvwri.begin();
          it!=_pvwri.end(); it++)
        delete it->second;
      _pvwri.clear();
      //  EPICS thread cleanup
      //  never
      //      ca_context_destroy();
    } break;
  default:
    break;
  }
  return tr;
}

InDatagram* Pds_TimeTool_event::TimeToolEpics::events     (InDatagram* dg)
{
  if (dg->seq.service()==TransitionId::Configure)
    iterate(&dg->xtc);
  else if (dg->seq.service()==TransitionId::L1Accept) {
    std::memcpy(&_pvts, &dg->seq.stamp(), sizeof(_pvts));
    iterate(&dg->xtc);
    ca_flush_io();
  }
  return dg;
}

int Pds_TimeTool_event::TimeToolEpics::process(Xtc* xtc)
{
  switch(xtc->contains.id()) {
  case TypeId::Id_Xtc:
    iterate(xtc);
    break;
  case TypeId::Id_TimeToolConfig:
    { ConfigCache* cache = ConfigCache::instance(xtc->contains, xtc->payload());
      char buff[64];
      if (cache) {
        if (_prefix) {
          sprintf(buff,"%s:%s:TTALL", _prefix, cache->base_name());
        } else {
          sprintf(buff,"%s:TTALL", cache->base_name());
        }
        _cfgs[xtc->src.phy()] = cache;
        _pvwri[xtc->src.phy()] = new PVWriter(buff);
      }
    } break;
  case TypeId::Id_TimeToolData:
    { ConfigCache* cache = _cfgs[xtc->src.phy()];
      if (cache && cache->data(xtc->contains, xtc->payload())) {
        if (cache->is_signal()) {
          PVWriter* pvw = _pvwri[xtc->src.phy()];
          if (pvw->connected()) {
            size_t nelems = pvw->data_size() / sizeof(double);
            double* v = reinterpret_cast<double*>(pvw->data());
            if (nelems >= 6) {
              v[0] = cache->position_pixel();
              v[1] = cache->position_time();
              v[2] = cache->amplitude();
              v[3] = cache->nxt_amplitude();
              v[4] = cache->ref_amplitude();
              v[5] = cache->position_fwhm();
              if (nelems >= 8) {
                v[6] = cache->signal_integral();
                v[7] = _pvts;
              }
              pvw->put();
            }
          }
        }
      }
    } break;
  default:
    break;
  }
  return 1;
}
