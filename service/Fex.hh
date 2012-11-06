#ifndef TimeTool_Fex_hh
#define TimeTool_Fex_hh

#include "Sideband.hh"
#include "Spectrum.hh"
#include "RefCorr.hh"
#include "Fit.hh"

#include <string>
using std::string;

namespace Pds { namespace Camera { class FrameV1; } }

namespace TimeTool {
  class RatioFit;

  class Fex {
  public:
    Fex(const char* fname="timetool.input");
    virtual ~Fex();
  public:
    void init_plots();
    void unconfigure();
    void configure();
    void reset();
    void analyze(const Pds::Camera::FrameV1& frame,
 		 bool bykik, bool no_laser);
    void analyze(const uint32_t* u_sig,
		 const uint32_t* u_sb,
		 const uint32_t* u_ref,
 		 bool bykik, bool no_laser);
  public:
    const string& base_name () const { return _base_name; }	
    double raw_position     () const { return _raw_position; }
    double filtered_position() const { return _flt_position; }
    double filtered_pos_ps  () const { return _flt_position_ps; }
    double filtered_pos_adj () const { return _flt_position_ps - _flt_center_ps; }
    double filtered_fwhm    () const { return _flt_fwhm; }
    double amplitude        () const { return _amplitude; }
    double next_amplitude   () const { return _nxt_amplitude; }
    double ref_amplitude    () const { return _ref_amplitude; }
    bool   status   () const { return _nfits>0; }
  public:
    bool   write_image      () const { return _write_image; }
    bool   write_projections() const { return _write_projections; }
    const uint32_t* signal_wf   () const { return sig; }
    const uint32_t* sideband_wf () const { return sb; }
    const uint32_t* reference_wf() const { return ref; }
  public:
    virtual void _monitor_raw_sig (const uint32_t*, double*) {}
    virtual void _monitor_ref_sig (const double*) {}
    virtual void _monitor_sub_sig (const double*) {}
    virtual void _monitor_flt_sig (const double*) {}
    virtual void _monitor_corr    (const RatioFit&, const RatioFit&) {}
  public:
    string   _fname;
    string   _base_name;
    unsigned _phy;

    unsigned _adjust_stats;
    string   _adjust_pv;

    unsigned _event_code_bykik;;
    unsigned _event_code_no_laser;;
	
    double   _calib_p0;
    double   _calib_p1;
    double   _calib_p2;

    bool     _projectX;
    unsigned _use_ref;
    unsigned _ref_fit_p1;
    unsigned _ref_top;
    unsigned _ref_bot;
    double   _ref_cut;
    unsigned _sig_top;
    unsigned _sig_bot;
    double   _sig_cut;
    unsigned _sb_top;
    unsigned _sb_bot;
    int      _spec_begin;
    int      _spec_end;
    double   _indicator_offset;
    unsigned _nwts;
    double*  _wts;
    double   _wtsig;

    bool     _write_image;
    bool     _write_projections;

    uint32_t*  sb;
    uint32_t*  ref;
    uint32_t*  sig;

    Sideband _sb;
    Spectrum _sp;
    Spectrum _ref;
    RefCorr  _ref_corr;

    enum {MaxFits=8};
    Fit      _fits[MaxFits];
    unsigned _nfits;

    double _raw_position;
    double _flt_position;
    double _flt_position_ps;
    double _flt_center_ps;
    double _flt_fwhm;
    double _amplitude;
    double _nxt_amplitude;
    double _ref_amplitude;
  };

};
#endif
