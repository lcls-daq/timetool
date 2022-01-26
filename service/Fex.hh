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
  class Fitter;
  class Fex {
  public:
    Fex(const char* fname="timetool.input",
        bool write_ref_auto=true,
        bool verbose=false,
        const char* ref_path=NULL);
    Fex(const Pds::Src&,
        const Pds::TimeTool::ConfigV1&,
        bool write_ref_auto=true,
        bool verbose=false,
        const char* ref_path=NULL);
    Fex(const Pds::Src&,
        const Pds::TimeTool::ConfigV2&,
        bool write_ref_auto=true,
        bool verbose=false,
        const char* ref_path=NULL);
    Fex(const Pds::Src&,
        const Pds::TimeTool::ConfigV3&,
        bool write_ref_auto=true,
        bool verbose=false,
        const char* ref_path=NULL);
    virtual ~Fex();
  public:
    void init_plots();
    void unconfigure();
    void configure();
    void reset();
    void analyze(const ndarray<const uint16_t,2>& frame,
                 const ndarray<const Pds::EvrData::FIFOEvent,1>& evr_fifo,
                 const Pds::Lusi::IpmFexV1* ipm);

    enum EventType { Dark, Reference, Signal };
    void analyze(EventType,
		 const ndarray<const int,1>& signal,
		 const ndarray<const int,1>& sideband);
    void analyze(EventType,
		 const ndarray<const int,2>& signal,
		 const ndarray<const int,2>& sideband);
    void analyze(EventType,
		 const ndarray<const int,1>& signal,
		 const ndarray<const int,1>& sideband,
		 const ndarray<const int,1>& reference);
    void analyze(EventType,
		 const ndarray<const int,2>& signal,
		 const ndarray<const int,2>& sideband,
		 const ndarray<const int,2>& reference);
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
    bool   use_full_roi     () const { return m_use_full_roi; }
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

    virtual void _monitor_raw_sig_full (const ndarray<const double,2>&) {}
    virtual void _monitor_ref_sig_full (const ndarray<const double,2>&) {}
    virtual void _monitor_sub_sig_full (const ndarray<const double,2>&) {}
    virtual void _monitor_flt_sig_full (const ndarray<const double,2>&) {}
  public:
    static const Pds::TimeTool::ConfigV3* config(const char* fname="timetool.input");
  public:
    string   _fname;
    string   _ref_path;

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

    bool     m_use_full_roi;   // use full roi region instead of projecting
    bool     m_use_fit;        // use a fit for the edge instead of an FIR

    unsigned m_sig_roi_lo[2];  // image sideband is projected within ROI
    unsigned m_sig_roi_hi[2];  // image sideband is projected within ROI

    unsigned m_sb_roi_lo[2];  // image sideband is projected within ROI
    unsigned m_sb_roi_hi[2];  // image sideband is projected within ROI

    unsigned m_ref_roi_lo[2];  // image sideband is projected within ROI
    unsigned m_ref_roi_hi[2];  // image sideband is projected within ROI

    unsigned m_frame_roi[2];  // frame data is an ROI

    bool     m_use_sb_roi;
    bool     m_use_ref_roi;

    double   m_sb_convergence ; // rolling average fraction (1/N)
    double   m_ref_convergence; // rolling average fraction (1/N)

    unsigned m_fit_max_iterations;    // maximum number of iterations for fitting
    double   m_fit_weights_factor;    // scale factor for deriving weights for fitting

    double   m_ref_offset;            // amount to subtract from the signal after dividing reference

    double   m_flt_offset;            // offset in pixels of the start of the filter result

    ndarray<double,1> m_fit_params;   // initial parameter values for fitting
    ndarray<double,1> m_weights;      // digital filter weights
    ndarray<double,1> m_ref_avg;      // accumulated reference
    ndarray<double,1> m_sb_avg;       // averaged sideband

    ndarray<double,2> m_ref_avg_full; // accumulated full reference
    ndarray<double,2> m_sb_avg_full;  // averaged full sideband

    ndarray<const int,1> m_sig;      // signal region projection
    ndarray<const int,1> m_sb;       // sideband region projection
    ndarray<const int,1> m_ref;      // reference region projection

    ndarray<const int,2> m_sig_full; // full signal region - spatial only
    ndarray<const int,2> m_sb_full;  // full sideband region - spatial only
    ndarray<const int,2> m_ref_full; // full reference region - spatial only

    unsigned m_pedestal;

    bool     _write_image;
    bool     _write_projections;
    bool     _write_ref_auto;

    double _flt_position;
    double _flt_position_ps;
    double _flt_fwhm;
    double _amplitude;
    double _nxt_amplitude;
    double _ref_amplitude;

    int _indicator_offset;

    std::vector<unsigned> _cut;

    Fitter* _fitter;
  };

};
#endif
