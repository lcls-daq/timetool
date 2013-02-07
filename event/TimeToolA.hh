#ifndef TimeToolA_event_hh
#define TimeToolA_event_hh

#include "pds/utility/Appliance.hh"
#include "pdsdata/xtc/XtcIterator.hh"

#include "timetool/service/Fex.hh"

namespace Pds { 
  namespace Opal1k  { class ConfigV1; } 
  namespace EvrData { class DataV3; } 
  namespace Lusi    { class IpmFexV1; } 
  namespace Camera  { class FrameV1; } 
};

namespace Pds_TimeTool_event {
  class TimeToolA : public Appliance,
		    public XtcIterator {
  public:
    TimeToolA();
    ~TimeToolA();
  public:
    Transition* transitions(Transition*);
    InDatagram* events     (InDatagram*);
    int         process    (Xtc* xtc);
  private:
    TimeTool::Fex _fex;
    const Camera::FrameV1* _frame;
    Pds::EvrData::DataV3* _evrdata;
    Pds::Lusi::IpmFexV1* _ipmdata;
    bool      _bykik;
    bool      _no_laser;
  };
};

#endif
