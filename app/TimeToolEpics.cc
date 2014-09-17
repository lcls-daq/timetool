#include "TimeToolEpics.hh"

#include "pds/epicstools/PVWriter.hh"
#include "pds/config/TimeToolConfigType.hh"
#include "pds/config/TimeToolDataType.hh"

#include "cadef.h"

using namespace Pds;
using Pds_Epics::PVWriter;

static bool _initialized=false;

TimeToolEpics::TimeToolEpics() {}

TimeToolEpics::~TimeToolEpics() {}

Transition* TimeToolEpics::transitions(Transition* tr)
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

InDatagram* TimeToolEpics::events     (InDatagram* dg)
{
  if (dg->seq.service()==TransitionId::Configure)
    iterate(&dg->xtc);
  else if (dg->seq.service()==TransitionId::L1Accept) {
    iterate(&dg->xtc);
    ca_flush_io();
  }
  return dg;
}

int TimeToolEpics::process(Xtc* xtc)
{
  switch(xtc->contains.id()) {
  case TypeId::Id_Xtc:
    iterate(xtc);
    break;
  case TypeId::Id_TimeToolConfig:
    { const TimeToolConfigType& c =
        *reinterpret_cast<const TimeToolConfigType*>(xtc->payload());
      char buff[64];
      sprintf(buff,"%s:TTALL",c.base_name());
      _pvwri[xtc->src.phy()] = new PVWriter(buff);
    } break;
  case TypeId::Id_TimeToolData:
    { const TimeToolDataType& d =
        *reinterpret_cast<const TimeToolDataType*>(xtc->payload());

      PVWriter* pvw = _pvwri[xtc->src.phy()];
      if (pvw->connected()) {
        double* v = reinterpret_cast<double*>(pvw->data());
        v[0] = d.position_pixel();
        v[1] = d.position_time();
        v[2] = d.amplitude();
        v[3] = d.nxt_amplitude();
        v[4] = d.ref_amplitude();
        v[5] = d.position_fwhm();
        pvw->put(); 
      }
    } break;
  default:
    break;
  }
  return 1;
}

