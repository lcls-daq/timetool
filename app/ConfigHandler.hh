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

  namespace TimeTool {
    class ConfigHandler : public XtcIterator {
    public:
      ConfigHandler(Appliance& app);
    public:
      int process(Xtc* xtc);
    public:
      void transitions(Transition* tr);
      void events     (InDatagram* dg);
    private:
      Appliance&        _app;
      GenericPool       _occPool;
      std::vector<Xtc>  _xtc;
      const Allocation* _allocation;
      const Transition* _configure;
    };
  };
};

#endif
