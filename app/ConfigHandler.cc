#include "timetool/app/ConfigHandler.hh"
#include "timetool/service/Fex.hh"

#include "pds/config/CfgCache.hh"
#include "pds/config/TimeToolConfigType.hh"
#include "pds/utility/Appliance.hh"
#include "pds/utility/Transition.hh"
#include "pds/xtc/InDatagram.hh"

namespace Pds {
  class TimeToolCfgCache : public CfgCache {
  public:
    TimeToolCfgCache(const Src& src, 
		     const TypeId& id,
		     int size) :
      CfgCache(src,id,size) {}
  public:
    int _size(void* p) const { return reinterpret_cast<TimeToolConfigType*>(p)->_sizeof(); }
  };
};

using Pds::Xtc;
using Pds::TypeId;
using Pds::Transition;
using Pds::TransitionId;
using Pds::Allocate;
using Pds::Allocation;
using Pds::UserMessage;
using Pds::InDatagram;
using Pds::TimeToolCfgCache;
using Pds::EvrCommandRequest;
using ::TimeTool::ConfigHandler;

ConfigHandler::ConfigHandler(Pds::Appliance& app) : 
  _app        (app),
  _occPool    (sizeof(UserMessage),4),
  _allocation (0),
  _configure  (0) 
{
}

int ConfigHandler::process(Xtc* xtc) 
{
  switch(xtc->contains.id()) {
  case TypeId::Id_Xtc:
    iterate(xtc);
    break;
  case TypeId::Id_Opal1kConfig:
    _xtc.push_back(Xtc(_timetoolConfigType,xtc->src));
			   
    break;
  default:
    break;
  }
  return 1;
}

void ConfigHandler::transitions(Transition* tr) 
{
  switch (tr->id()) {
  case TransitionId::Configure:
    _configure = new Transition(tr->id(), 
				tr->phase(),
				tr->sequence(),
				tr->env());
    break;
  case TransitionId::Unconfigure:
    delete _configure;
    break;
  case TransitionId::Map:
    { const Allocation& alloc = reinterpret_cast<const Allocate*>(tr)->allocation();
      _allocation = new Allocation(alloc);
    } break;
  case TransitionId::Unmap:
    delete _allocation;
    break;
  default:
    break;
  }
}

static void _add_config(const TimeToolConfigType& cfg,
                        std::vector<unsigned>& requested_codes) {
#define INSERT_CODE(c) {                                        \
    if (c) {                                                    \
      bool lfound=false;                                        \
      unsigned co=abs(c);                                       \
      for(unsigned i=0; i<requested_codes.size(); i++)          \
        if (requested_codes[i]==co) { lfound=true; break; }     \
      if (!lfound) requested_codes.push_back(co); } }

  for(unsigned k=0; k<cfg.beam_logic().size(); k++)
    INSERT_CODE(cfg.beam_logic()[k].event_code());
  for(unsigned k=0; k<cfg.laser_logic().size(); k++)
    INSERT_CODE(cfg.laser_logic()[k].event_code());

  Pds::TimeTool::dump(cfg);
#undef INSERT_CODE
}

void ConfigHandler::events(InDatagram* dg) 
{
  if (dg->seq.service()!=TransitionId::Configure)
    return;

  _xtc.clear();
  process(&dg->xtc);

  if (_xtc.size()) {
    std::vector<unsigned> requested_codes;
    TimeToolConfigType tmplate(4,4,256,8,64);
    for(unsigned i=0; i<_xtc.size(); i++) {
      TimeToolCfgCache cache(_xtc[i].src,
			     _xtc[i].contains,
			     tmplate._sizeof());
      cache.init(*_allocation);
      if (cache.fetch(_configure) > 0) {
	cache.record(dg);
        const TimeToolConfigType& cfg = 
          *reinterpret_cast<const TimeToolConfigType*>(cache.current());
        _add_config(cfg, requested_codes);
      }
    }

    if (!requested_codes.size()) {
      const TimeToolConfigType* cfg = Fex::config();
      if (cfg) {
        Xtc xtc(_timetoolConfigType,_xtc[0].src);
        xtc.alloc(cfg->_sizeof());
        dg->insert(xtc,cfg);
        _add_config(*cfg, requested_codes);
        delete[] reinterpret_cast<const char*>(cfg);
      }
    }

    if (requested_codes.size())
      _app.post(new(&_occPool) EvrCommandRequest(requested_codes));
  }
}
