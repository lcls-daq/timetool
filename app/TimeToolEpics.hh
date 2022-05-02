#ifndef TimeToolEpics_hh
#define TimeToolEpics_hh

#include "pds/utility/Appliance.hh"
#include "pdsdata/xtc/XtcIterator.hh"

#include <map>

namespace Pds_Epics { class PVWriter; }
namespace TimeTool { class ConfigCache; }

namespace Pds {
  class TimeToolEpics : public Appliance,
                        public XtcIterator {
  public:
    TimeToolEpics();
    ~TimeToolEpics();
  public:
    Transition* transitions(Transition*);
    InDatagram* events     (InDatagram*);
  public:
    int         process    (Xtc*);
  private:
    std::map<uint32_t,Pds_Epics::PVWriter*> _pvwri;
    std::map<uint32_t,TimeTool::ConfigCache*> _cfgs;
    double _pvts;
  };
};

#endif
