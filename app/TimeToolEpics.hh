#ifndef TimeToolEpics_hh
#define TimeToolEpics_hh

#include "pds/utility/Appliance.hh"
#include "pdsdata/xtc/XtcIterator.hh"

#include <map>

namespace Pds_Epics { class PVWriter; }

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
  };
};

#endif
