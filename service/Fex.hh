#ifndef TimeTool_Fex_hh
#define TimeTool_Fex_hh

#include "pdsdata/xtc/Src.hh"
#include "pdsdata/psddl/camera.ddl.h"
#include "pdsdata/psddl/evr.ddl.h"
#include "pdsdata/psddl/lusi.ddl.h"
#include "pdsdata/psddl/timetool.ddl.h"
#include "ndarray/ndarray.h"

#include <string>
using std::string;

#include <vector>

namespace Pds { 
  namespace Camera { class FrameV1; } 
};

namespace TimeTool {
  class Fex {
  public:
    Fex(const char* fname="timetool.input");
    Fex(const Pds::Src&, const Pds::TimeTool::ConfigV1&);
    virtual ~Fex();
  public:
    void init_plots();
    void unconfigure();
    void configure();
    void reset();
    void analyze(const ndarray<const uint16_t,2>& frame,
                 const ndarray<const Pds::EvrData::FIFOEvent,1>& evr_fifo,
                 const Pds::Lusi::IpmFexV1* ipm);
    void analyze(Pds::TimeTool::DataV1::EventType,
		 const ndarray<const int,1>& signal,
		 const ndarray<const int,1>& sideband);
  public:
    const string& base_name () const { return m_put_key; }	
    const Pds::Src&      src() const { return _src; }
    //    double raw_position     () const { return _raw_position; }
    double filtered_position() const { return _flt_position; }
    double filtered_pos_ps  () const { return _flt_position_ps; }
    double filtered_fwhm    () const { return _flt_fwhm; }
    double amplitude        () const { return _amplitude; }
    double next_amplitude   () const { return _nxt_amplitude; }
    double ref_amplitude    () const { return _ref_amplitude; }
    bool   status   () const { return _flt_fwhm>0; }
  public:
    bool   write_image      () const { return _write_image; }
    bool   write_projections() const { return _write_projections; }
//     const uint32_t* signal_wf   () const { return sig; }
//     const uint32_t* sideband_wf () const { return sb; }
//     const uint32_t* reference_wf() const { return ref; }
  public:
    virtual void _monitor_raw_sig (const ndarray<const double,1>&) {}
    virtual void _monitor_ref_sig (const ndarray<const double,1>&) {}
    virtual void _monitor_sub_sig (const ndarray<const double,1>&) {}
    virtual void _monitor_flt_sig (const ndarray<const double,1>&) {}
  public:
    static const Pds::TimeTool::ConfigV1* config(const char* fname="timetool.input");
  public:
    string   _fname;

    unsigned m_get_key;
    string   m_put_key;
    Pds::Src _src;

    ndarray<Pds::TimeTool::EventLogic,1> m_beam_logic;
    ndarray<Pds::TimeTool::EventLogic,1> m_laser_logic;

    Pds::Src m_ipm_get_key;  // use this ipm threshold for detecting no beam
    double   m_ipm_beam_threshold;

    std::vector<double> m_calib_poly;

    bool     m_projectX ;  // project image onto X axis
    int      m_proj_cut ;  // valid projection must be at least this large

    unsigned m_sig_roi_lo[2];  // image sideband is projected within ROI
    unsigned m_sig_roi_hi[2];  // image sideband is projected within ROI

    unsigned m_sb_roi_lo[2];  // image sideband is projected within ROI
    unsigned m_sb_roi_hi[2];  // image sideband is projected within ROI

    unsigned m_frame_roi[2];  // frame data is an ROI

    double   m_sb_convergence ; // rolling average fraction (1/N)
    double   m_ref_convergence; // rolling average fraction (1/N)

    ndarray<double,1> m_weights; // digital filter weights
    ndarray<double,1> m_ref;     // accumulated reference
    ndarray<double,1> m_sb_avg;  // averaged sideband

    ndarray<const int,1> m_sig;     // signal region projection
    ndarray<const int,1> m_sb;      // sideband region projection

    unsigned m_pedestal;

    bool     _write_image;
    bool     _write_projections;

    double _flt_position;
    double _flt_position_ps;
    double _flt_fwhm;
    double _amplitude;
    double _nxt_amplitude;
    double _ref_amplitude;

    int _indicator_offset;

    std::vector<unsigned> _cut;
  };

};
#endif
