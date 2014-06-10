#include "Fex.hh"
#include "Config.hh"

#include "pdsdata/psddl/camera.ddl.h"
#include "pdsdata/psddl/opal1k.ddl.h"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/BldInfo.hh"
#include "psalg/psalg.h"

#include <list>
#include <sstream>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

using Pds::DetInfo;
using namespace TimeTool;

//#define DBUG

Fex::Fex(const char* fname) :
  _fname(fname+strspn(fname," \t"))
{
}

Fex::~Fex() 
{
}

void Fex::init_plots()
{
  _monitor_ref_sig( m_ref );
}

void Fex::unconfigure()
{
  // Record accumulated reference
  char buff[128];
  const char* dir = getenv("HOME");
  sprintf(buff,"%s/timetool.ref.%08x", dir ? dir : "/tmp", m_get_key);
  FILE* f = fopen(buff,"w");
  if (f) {
    for(unsigned i=0; i<m_ref.size(); i++)
      fprintf(f," %f",m_ref[i]);
    fprintf(f,"\n");
    fclose(f);
  }
}

void Fex::configure()
{
  char buff[128];
  if (_fname[0]=='/')
    strcpy(buff,_fname.c_str());
  else {
    const char* dir = getenv("HOME");
    sprintf(buff,"%s/%s", dir ? dir : "/tmp", _fname.c_str());
  }

  printf("TimeTool:Fex reading configuration from file %s\n",buff);
  Config svc(buff);

  m_put_key = svc.config("base_name",m_put_key);
  unsigned phy = svc.config("phy",m_get_key);
  if (phy) {
    _src = DetInfo(getpid(),
                   (DetInfo::Detector)((phy>>24)&0xff), ((phy>>16)&0xff),
                   (DetInfo::Device  )((phy>> 8)&0xff), ((phy>> 0)&0xff));
  }
  else {
    std::string s = svc.config("phy",std::string());
    Pds::DetInfo det(s.c_str());
    if (det.detector()<Pds::DetInfo::NumDetector) {
      _src = det;
    }
    else {
      _src = Pds::BldInfo(s.c_str());
    }
  }
  m_get_key = _src.phy();
  
  m_event_code_no_beam   = svc.config("event_code_bykik"   ,m_event_code_no_beam);
  m_event_code_no_laser  = svc.config("event_code_no_laser",m_event_code_no_laser);

  {
    std::string s = svc.config("ipm_beam_src",std::string());
    if (s.empty())
      m_ipm_get_key   = Pds::DetInfo("None");
    else {
      Pds::DetInfo det(s.c_str());
      if (det.detector()<Pds::DetInfo::NumDetector)
        m_ipm_get_key = det;
      else
        m_ipm_get_key = Pds::BldInfo(s.c_str());
    }
  }

  m_ipm_beam_threshold  = svc.config("ipm_beam_threshold",0.);
  m_calib_poly          = svc.config("calib_poly",m_calib_poly);

  {
    std::string a = svc.config("project",std::string("X"));
    m_projectX = (a[0]=='x' || a[0]=='X');
    printf("project %c [%c]\n", m_projectX ? 'X' : 'Y',a[0]);
  }

  m_proj_cut        = svc.config("sig_cut" ,0);
  m_proj_cut        = svc.config("proj_cut",m_proj_cut);

  m_frame_roi[0] = m_frame_roi[1] = 0;

  std::vector<unsigned> sig_roi_x(2);
  std::vector<unsigned> sig_roi_y(2);
  std::vector<unsigned> sb_roi_x(2);
  std::vector<unsigned> sb_roi_y(2);

  if (m_projectX) {
    sig_roi_y[0]     = svc.config("sig_top",0);
    sig_roi_y[1]     = svc.config("sig_bot",1023);
    sb_roi_y[0]      = svc.config("sb_top",0);
    sb_roi_y[1]      = svc.config("sb_bot",0);
    sig_roi_x[0]     = sb_roi_x[0] = svc.config("spec_begin",0);
    sig_roi_x[1]     = sb_roi_x[1] = svc.config("spec_end",1023);
  }
  else {
    sig_roi_x[0]     = svc.config("sig_top",0);
    sig_roi_x[1]     = svc.config("sig_bot",1023);
    sb_roi_x[0]      = svc.config("sb_top",0);
    sb_roi_x[1]      = svc.config("sb_bot",0);
    sig_roi_y[0]     = sb_roi_y[0] = svc.config("spec_begin",0);
    sig_roi_y[1]     = sb_roi_y[1] = svc.config("spec_end",1023);
  }

  if (sb_roi_y[0]!=sb_roi_y[1] &&
      sb_roi_x[0]!=sb_roi_x[1]) {
    if (m_projectX) {
      if (sb_roi_x[0]!=sig_roi_x[0] ||
          sb_roi_x[1]!=sig_roi_x[1]) {
        sb_roi_x[0] = sig_roi_x[0];
        sb_roi_x[1] = sig_roi_x[1];
        printf("TimeTool: Signal and sideband roi x ranges differ.  Setting sideband roi to signal.\n");
      }
      if ((sb_roi_y[1]-sb_roi_y[0]) != (sig_roi_y[1]-sig_roi_y[0])) {
        throw std::string("TimeTool: Signal and sideband roi y range sizes differ.");
      }
    }
    else {
      if (sb_roi_y[0]!=sig_roi_y[0] ||
          sb_roi_y[1]!=sig_roi_y[1]) {
        sb_roi_y[0] = sig_roi_y[0];
        sb_roi_y[1] = sig_roi_y[1];
        printf("TimeTool: Signal and sideband roi y ranges differ.  Setting sideband roi to signal.\n");
      }
      if ((sb_roi_x[1]-sb_roi_x[0]) != (sig_roi_x[1]-sig_roi_x[0])) {
        throw std::string("TimeTool: Signal and sideband roi x range sizes differ.");
      }
    }
    m_sb_roi_lo[0] = sb_roi_y[0];
    m_sb_roi_hi[0] = sb_roi_y[1];
    m_sb_roi_lo[1] = sb_roi_x[0];
    m_sb_roi_hi[1] = sb_roi_x[1];
  }
  else {
    m_sb_roi_lo[0] = m_sb_roi_hi[0] = 0;
    m_sb_roi_lo[1] = m_sb_roi_hi[1] = 0;
  }

  m_sig_roi_lo[0] = sig_roi_y[0];
  m_sig_roi_hi[0] = sig_roi_y[1];
  m_sig_roi_lo[1] = sig_roi_x[0];
  m_sig_roi_hi[1] = sig_roi_x[1];

  m_sb_convergence  = svc.config("sb_convergence",0.05);
  m_ref_convergence = svc.config("ref_convergence",1.);
  _indicator_offset= svc.config("indicator_offset",0);

  {
    std::vector<double> w = svc.config("weights",std::vector<double>());
    m_weights = make_ndarray<double>(w.size());
    //  Reverse the ordering of the weights for the
    //  psalg::finite_impulse_response implementation
    for(unsigned i=0; i<w.size(); i++)
      m_weights[i] = w[w.size()-i-1];
  }

  _write_image       = svc.config("write_image",true);
  _write_projections = svc.config("write_projections",false);

  // Load reference
  { const char* dir = getenv("HOME");
    sprintf(buff,"%s/timetool.ref.%08x", dir ? dir : "/tmp", m_get_key);
    std::vector<double> r;
    FILE* rf = fopen(buff,"r");
    if (rf) {
      float rv;
      while( fscanf(rf,"%f",&rv)>0 )
        r.push_back(rv);
      if (r.size()==sig_roi_x[1]-sig_roi_x[0]+1) {
        m_ref = make_ndarray<double>(r.size());
        for(unsigned i=0; i<r.size(); i++)
          m_ref[i] = r[i];
      }
      else {
        printf("Reference in %s size %zu does not match spec_begin/end[%u/%u]\n",
               buff, r.size(), sig_roi_x[0], sig_roi_x[1]);
      }
      fclose(rf);
    }
  }

  m_pedestal = 32;
}

void Fex::reset() 
{
  _flt_position  = 0;
  _flt_position_ps = 0;
  _flt_fwhm      = 0;
  _amplitude     = 0;
  _ref_amplitude = 0;
  _nxt_amplitude = -1;
}

void Fex::analyze(const ndarray<const uint16_t,2>& f,
                  const ndarray<const Pds::EvrData::FIFOEvent,1>& evr,
                  const Pds::Lusi::IpmFexV1* ipm)
{
  bool nobeam  = false;
  bool nolaser = false;
  //
  //  Beam is absent if BYKIK fired
  //
  unsigned ec_nobeam  = unsigned(abs(m_event_code_no_beam));
  unsigned ec_nolaser = unsigned(abs(m_event_code_no_laser));
  for(unsigned i=0; i<evr.shape()[0]; i++) {
    nobeam  |= (evr[i].eventCode()==ec_nobeam);
    nolaser |= (evr[i].eventCode()==ec_nolaser);
  }

  if (m_event_code_no_beam  < 0) nobeam  =!nobeam;
  if (m_event_code_no_laser < 0) nolaser =!nolaser;

  if (nolaser) return;

  //
  //  Beam is absent if not enough signal on the IPM detector
  //
  if (ipm)
    nobeam |= ipm->sum() < m_ipm_beam_threshold;

  if (!f.size()) return;

  std::string msg;
  for(unsigned i=0; i<2; i++) {
    if (m_sig_roi_hi[i] >= f.shape()[i]) {
      if (m_projectX == (i==0)) {
        std::stringstream s;
        s << "Timetool: signal " << (i==0 ? 'Y':'X') << " upper bound ["
          << m_sig_roi_hi[i] << "] exceeds frame bounds ["
          << f.shape()[i] << "].";
        msg += s.str();
      }
      m_sig_roi_hi[i] = f.shape()[i]-1;
    }
    if (m_sb_roi_hi[i] >= f.shape()[i]) {
      if (m_projectX == (i==0)) {
        std::stringstream s;
        s << "Timetool: sideband " << (i==0 ? 'Y':'X') << " upper bound ["
          << m_sb_roi_hi[i] << "] exceeds frame bounds ["
          << f.shape()[i] << "].";
        msg += s.str();
      }
      m_sb_roi_hi[i] = f.shape()[i]-1;
    }
  }
  if (!msg.empty())
    throw msg;

  //
  //  Project signal ROI
  //
  unsigned pdim = m_projectX ? 1:0;
  ndarray<const int,1> sig = psalg::project(f,
                                            m_sig_roi_lo,
                                            m_sig_roi_hi,
                                            m_pedestal, pdim);

  //
  //  Calculate sideband correction
  //
  ndarray<const int,1> sb;
  if (m_sb_roi_lo[0]!=m_sb_roi_hi[0])
    sb = psalg::project(f,
                        m_sb_roi_lo ,
                        m_sb_roi_hi,
                        m_pedestal, pdim);

  ndarray<double,1> sigd = make_ndarray<double>(sig.shape()[0]);

  //
  //  Correct projection for common mode found in sideband
  //
  if (sb.size()) {
    psalg::rolling_average(sb, m_sb_avg, m_sb_convergence);

    ndarray<const double,1> sbc = psalg::commonModeLROE(sb, m_sb_avg);

    for(unsigned i=0; i<sig.shape()[0]; i++)
      sigd[i] = double(sig[i])-sbc[i];
  }
  else {
    for(unsigned i=0; i<sig.shape()[0]; i++)
      sigd[i] = double(sig[i]);
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  for(unsigned i=0; i<sig.shape()[0]; i++)
    if (sigd[i]>m_proj_cut)
      lcut=false;

  if (lcut) return;

  if (nobeam) {
    _monitor_ref_sig( sigd );
    psalg::rolling_average(ndarray<const double,1>(sigd),
                           m_ref, m_ref_convergence);
    return;
  }

  _monitor_raw_sig( sigd );

  if (m_ref.size()==0)
    return;

  //
  //  Divide by the reference
  //
  for(unsigned i=0; i<sig.shape()[0]; i++)
    sigd[i] = sigd[i]/m_ref[i] - 1;

  _monitor_sub_sig( sigd );

  //
  //  Apply the digital filter
  //
  ndarray<double,1> qwf = psalg::finite_impulse_response(m_weights,sigd);

  _monitor_flt_sig( qwf );

  //
  //  Find the two highest peaks that are well-separated
  //
  const double afrac = 0.50;
  std::list<unsigned> peaks =
    psalg::find_peaks(qwf, afrac, 2);

  unsigned nfits = peaks.size();
  if (nfits>0) {
    unsigned ix = *peaks.begin();
    ndarray<double,1> pFit0 = psalg::parab_fit(qwf,ix,0.8);
    if (pFit0[2]>0) {
      double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim];

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = pFit0[0];
      _flt_position  = xflt;
      _flt_position_ps  = xfltc;
      _flt_fwhm      = pFit0[2];
      _ref_amplitude = m_ref[ix];

      if (nfits>1) {
        ndarray<double,1> pFit1 =
          psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
        if (pFit1[2]>0)
          _nxt_amplitude = pFit1[0];
      }
    }
  }
}
