#ifndef TimeToolC_hh
#define TimeToolC_hh

#include "pds/client/WorkThreads.hh"

#include "pds/utility/Transition.hh"
#include "pds/service/GenericPool.hh"
#include "pdsdata/psddl/evr.ddl.h"
#include "ndarray/ndarray.h"

namespace TimeTool { class ConfigHandler; }

namespace Pds {
  class ConfigHandler;
  class FexApp;

  class TimeToolC : public WorkThreads {
  public:
    TimeToolC();
    ~TimeToolC();
  public:
    Transition* transitions(Transition*);
    InDatagram* events     (InDatagram*);
    Occurrence* occurrences(Occurrence*);
  private:
    std::vector< std::vector<Pds::EvrData::FIFOEvent> > _evr;
    TimeTool::ConfigHandler*                            _config;
    Pds::GenericPool                                    _pool;
  };
};

#endif
