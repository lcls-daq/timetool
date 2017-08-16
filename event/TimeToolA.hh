#ifndef TimeToolA_event_hh
#define TimeToolA_event_hh

#include "pds/utility/Appliance.hh"
#include "pdsdata/xtc/XtcIterator.hh"

#include "timetool/service/Fex.hh"

#include <boost/shared_ptr.hpp>

namespace Pds { 
  namespace Opal1k  { class ConfigV1; } 
  namespace EvrData { class DataV3; } 
  namespace Lusi    { class IpmFexV1; } 
  namespace Camera  { class FrameV1; } 
};

namespace Pds_TimeTool_event {
  class TimeToolA : public Pds::Appliance,
		    public Pds::XtcIterator {
  public:
    TimeToolA();
    ~TimeToolA();
  public:
    Pds::Transition* transitions(Pds::Transition*);
    Pds::InDatagram* events     (Pds::InDatagram*);
    int         process    (Pds::Xtc* xtc);
    const ::TimeTool::Fex& fex() const { return _fex; }
  private:
    ::TimeTool::Fex _fex;
    const Pds::Camera::FrameV1* _frame;
    Pds::EvrData::DataV3* _evrdata;
    Pds::Lusi::IpmFexV1* _ipmdata;
    bool      _bykik;
    bool      _no_laser;
    boost::shared_ptr<Pds::Xtc> _pXtc;
  };
};

#endif
