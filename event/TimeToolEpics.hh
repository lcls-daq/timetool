#ifndef TimeToolEpics_event_hh
#define TimeToolEpics_event_hh

#include "pds/utility/Appliance.hh"
#include "pdsdata/xtc/XtcIterator.hh"

#include <map>

namespace Pds_Epics { class PVWriter; }
namespace TimeTool { class ConfigCache; }

namespace Pds_TimeTool_event {
  class TimeToolEpics : public Pds::Appliance,
                        public Pds::XtcIterator {
  public:
    TimeToolEpics(const char* base_name=NULL);
    ~TimeToolEpics();
  public:
    Pds::Transition* transitions(Pds::Transition*);
    Pds::InDatagram* events     (Pds::InDatagram*);
  public:
    int         process    (Pds::Xtc*);
  private:
    std::map<uint32_t,Pds_Epics::PVWriter*> _pvwri;
    std::map<uint32_t,TimeTool::ConfigCache*> _cfgs;
    double _pvts;
    const char* _prefix;
  };
};

#endif
