#include "timetool/app/ConfigHandler.hh"

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

using namespace Pds;

TimeTool::ConfigHandler::ConfigHandler(Appliance& app) : 
  _app        (app),
  _occPool    (sizeof(UserMessage),4),
  _allocation (0),
  _configure  (0) 
{
}

int TimeTool::ConfigHandler::process(Xtc* xtc) 
{
  switch(xtc->contains.id()) {
  case TypeId::Id_Xtc:
    iterate(xtc);
    break;
  case TypeId::Id_Opal1kConfig:
    _xtc.push_back(Xtc(TypeId(TypeId::Id_TimeToolConfig,1),
		       xtc->src));
			   
    break;
  default:
    break;
  }
  return 1;
}

void TimeTool::ConfigHandler::transitions(Transition* tr) 
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

void TimeTool::ConfigHandler::events(InDatagram* dg) 
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

#define INSERT_CODE(c) {                                                \
          if (c) {                                                      \
            bool lfound=false;                                          \
            unsigned co=abs(c);                                         \
            for(unsigned i=0; i<requested_codes.size(); i++)            \
              if (requested_codes[i]==co) { lfound=true; break; }       \
            if (!lfound) requested_codes.push_back(co); } }

	const TimeToolConfigType& cfg = 
	  *reinterpret_cast<const TimeToolConfigType*>(cache.current());
	for(unsigned k=0; k<cfg.beam_logic().size(); k++)
	  INSERT_CODE(cfg.beam_logic()[k].event_code());
	for(unsigned k=0; k<cfg.laser_logic().size(); k++)
	  INSERT_CODE(cfg.laser_logic()[k].event_code());

	TimeTool::dump(cfg);

#undef INSERT_CODE
      }
    }

    if (requested_codes.size())
      _app.post(new(&_occPool) EvrCommandRequest(requested_codes));
  }
}
