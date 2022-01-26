#ifndef TimeToolC_event_hh
#define TimeToolC_event_hh

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
  class TimeToolC : public Pds::Appliance,
		    public Pds::XtcIterator {
  public:
    TimeToolC();
    TimeToolC(const Pds::Src&,
              bool write_ref_auto=true,
              bool verbose=false,
              const char* ref_path=NULL);
    TimeToolC(const char*,
              bool write_ref_auto=true,
              bool verbose=false,
              const char* ref_path=NULL);
    ~TimeToolC();
  public:
    Pds::Transition* transitions(Pds::Transition*);
    Pds::InDatagram* events     (Pds::InDatagram*);
    int         process    (Pds::Xtc* xtc);
    const ::TimeTool::Fex* fex() const { return _fex; }
  private:
    Pds::Src _src;
    ::TimeTool::Fex* _fex;
    const Pds::Camera::FrameV1* _frame;
    Pds::EvrData::DataV3* _evrdata;
    Pds::Lusi::IpmFexV1* _ipmdata;
    const char* _ref_path;
    bool      _write_ref_auto;
    bool      _verbose;
    bool      _bykik;
    bool      _no_laser;
    boost::shared_ptr<Pds::Xtc> _pXtc;
  };
};

#endif
