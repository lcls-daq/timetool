#ifndef TimeTool_ConfigHandler_hh
#define TimeTool_ConfigHandler_hh

#include "pdsdata/xtc/XtcIterator.hh"

#include "pds/service/GenericPool.hh"
#include "pdsdata/xtc/Xtc.hh"

#include <vector>

namespace Pds {
  class Allocation;
  class Appliance;
  class InDatagram;
  class Transition;
};

namespace TimeTool {
  class ConfigHandler : public Pds::XtcIterator {
  public:
    ConfigHandler(Pds::Appliance& app);
  public:
    int process(Pds::Xtc* xtc);
  public:
    void transitions(Pds::Transition* tr);
    void events     (Pds::InDatagram* dg);
  private:
    Pds::Appliance&        _app;
    Pds::GenericPool       _occPool;
    std::vector<Pds::Xtc>  _xtc;
    const Pds::Allocation* _allocation;
    const Pds::Transition* _configure;
  };
};

#endif
