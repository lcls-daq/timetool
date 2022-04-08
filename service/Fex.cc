#include "Fex.hh"
#include "Config.hh"
#include "Fitter.hh"

#include "pdsdata/psddl/opal1k.ddl.h"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/BldInfo.hh"
#include "psalg/psalg.h"

#include <list>
#include <sstream>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dirent.h>

//#define DBUG

static const char* default_ref_path()
{
  static char path[PATH_MAX];
  const char* hdir = getenv("HOME");
  if (hdir) {
    sprintf(path,"%s/timetool", hdir);
    DIR *dir = opendir(path);
    if (dir) {
      closedir(dir);
      return path;
    } else {
      return hdir;
    }
  } else {
    return "/tmp";
  }
}

typedef Pds::TimeTool::ConfigV3  TimeToolConfigType;

using Pds::DetInfo;
using namespace TimeTool;

enum Cuts { NCALLS, NOLASER, FRAMESIZE, PROJCUT,
            NOBEAM, NOREF, NOFITS, NCUTS };
static const char* cuts[] = {"NCalls",
                             "NoLaser",
                             "FrameSize",
                             "ProjCut",
                             "NoBeam",
                             "NoRef",
                             "NoFits",
                             NULL };

static ndarray<double,1> load_reference(unsigned key, unsigned sz, const char* dir);

static ndarray<double,2> load_reference(unsigned key, unsigned row_sz, unsigned col_sz, const char* dir);


Fex::Fex(const char* fname,
         bool write_ref_auto,
         bool verbose,
         const char* ref_path) :
  _fname(fname+strspn(fname," \t")),
  _ref_path(ref_path ? ref_path : default_ref_path()),
  _write_ref_auto(write_ref_auto),
  _fitter(new Fitter(verbose))
{
}

Fex::Fex(const Pds::Src& src,
         const Pds::TimeTool::ConfigV1& cfg,
         bool write_ref_auto,
         bool verbose,
         const char* ref_path) :
  _ref_path(ref_path ? ref_path : default_ref_path()),
  _write_ref_auto(write_ref_auto),
  _fitter(new Fitter(verbose))
{
  //  m_put_key = std::string(cfg.base_name(),
  //                          cfg.base_name_length());
  m_put_key = std::string(cfg.base_name());

  _src = src;
  m_get_key = src.phy();

  m_beam_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.beam_logic().size());
  std::copy(cfg.beam_logic().begin(),
            cfg.beam_logic().end(),
            m_beam_logic.begin());

  m_laser_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.laser_logic().size());
  std::copy(cfg.laser_logic().begin(),
            cfg.laser_logic().end(),
            m_laser_logic.begin());

  m_calib_poly.resize(cfg.calib_poly_dim());
  std::copy(cfg.calib_poly().begin(),cfg.calib_poly().end(),m_calib_poly.data());

  m_projectX = (cfg.project_axis() == Pds::TimeTool::ConfigV1::X);

  m_proj_cut = cfg.signal_cut();

  m_use_full_roi = false;
  m_use_fit = false;

  m_frame_roi[0] = m_frame_roi[1] = 0;

  m_sig_roi_lo[0] = cfg.sig_roi_lo().row();
  m_sig_roi_lo[1] = cfg.sig_roi_lo().column();

  m_sig_roi_hi[0] = cfg.sig_roi_hi().row();
  m_sig_roi_hi[1] = cfg.sig_roi_hi().column();

  if (cfg.subtract_sideband()) {
    m_use_sb_roi = true;
    m_sb_roi_lo[0] = cfg.sb_roi_lo().row();
    m_sb_roi_lo[1] = cfg.sb_roi_lo().column();

    m_sb_roi_hi[0] = cfg.sb_roi_hi().row();
    m_sb_roi_hi[1] = cfg.sb_roi_hi().column();
  }
  else {
    m_use_sb_roi = false;
    m_sb_roi_lo[0] = m_sb_roi_hi[0] = 0;
    m_sb_roi_lo[1] = m_sb_roi_hi[1] = 0;
  }

  m_ref_roi_lo[0] = m_ref_roi_hi[0] = 0;
  m_ref_roi_lo[1] = m_ref_roi_hi[1] = 0;

  m_sb_convergence  = cfg.sb_convergence();
  m_ref_convergence = cfg.ref_convergence();

  m_weights = make_ndarray<double>(cfg.number_of_weights());
  std::copy(cfg.weights().begin(), cfg.weights().end(), m_weights.data());

  _indicator_offset = m_use_fit ? 0 : -cfg.number_of_weights()/2;

  m_fit_max_iterations = 0;
  m_fit_weights_factor = 0.;
  m_fit_params = ndarray<double,1>();

  _write_image = cfg.write_image();
  _write_projections = cfg.write_projections();

  unsigned sz = m_projectX ?
    m_sig_roi_hi[1]-m_sig_roi_lo[1]+1 :
    m_sig_roi_hi[0]-m_sig_roi_lo[0]+1;

  if (m_use_fit)
    _fitter->configure(sz,
                       m_fit_max_iterations,
                       m_fit_weights_factor,
                       m_fit_params);

  m_ref_offset = m_use_fit ? 0.0 : 1.0;

  m_flt_offset = m_use_fit ? 0 : m_weights.size() / 2;

  m_ref_avg = load_reference(m_get_key,sz,_ref_path.c_str());
  m_sig  = ndarray<const int,1>();
  m_sb   = ndarray<const int,1>();
  m_ref  = ndarray<const int,1>();

  m_ref_avg_full = ndarray<double,2>();
  m_sig_full = ndarray<const int,2>();
  m_sb_full  = ndarray<const int,2>();
  m_ref_full = ndarray<const int,2>();

  m_pedestal = 32;

  _cut.clear();
  _cut.resize(NCUTS,0);
}

Fex::Fex(const Pds::Src& src,
         const Pds::TimeTool::ConfigV2& cfg,
         bool write_ref_auto,
         bool verbose,
         const char* ref_path) :
  _ref_path(ref_path ? ref_path : default_ref_path()),
  _write_ref_auto(write_ref_auto),
  _fitter(new Fitter(verbose))
{
  //  m_put_key = std::string(cfg.base_name(),
  //                          cfg.base_name_length());
  m_put_key = std::string(cfg.base_name());

  _src = src;
  m_get_key = src.phy();

  m_beam_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.beam_logic().size());
  std::copy(cfg.beam_logic().begin(),
            cfg.beam_logic().end(),
            m_beam_logic.begin());

  m_laser_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.laser_logic().size());
  std::copy(cfg.laser_logic().begin(),
            cfg.laser_logic().end(),
            m_laser_logic.begin());

  m_calib_poly.resize(cfg.calib_poly_dim());
  std::copy(cfg.calib_poly().begin(),cfg.calib_poly().end(),m_calib_poly.data());

  m_projectX = (cfg.project_axis() == Pds::TimeTool::ConfigV2::X);

  m_proj_cut = cfg.signal_cut();

  m_use_full_roi = false;
  m_use_fit = false;

  m_frame_roi[0] = m_frame_roi[1] = 0;

  m_sig_roi_lo[0] = cfg.sig_roi_lo().row();
  m_sig_roi_lo[1] = cfg.sig_roi_lo().column();

  m_sig_roi_hi[0] = cfg.sig_roi_hi().row();
  m_sig_roi_hi[1] = cfg.sig_roi_hi().column();

  if (cfg.subtract_sideband()) {
    m_use_sb_roi = true;
    m_sb_roi_lo[0] = cfg.sb_roi_lo().row();
    m_sb_roi_lo[1] = cfg.sb_roi_lo().column();

    m_sb_roi_hi[0] = cfg.sb_roi_hi().row();
    m_sb_roi_hi[1] = cfg.sb_roi_hi().column();
  }
  else {
    m_use_sb_roi = false;
    m_sb_roi_lo[0] = m_sb_roi_hi[0] = 0;
    m_sb_roi_lo[1] = m_sb_roi_hi[1] = 0;
  }

  if (cfg.use_reference_roi()) {
    m_use_ref_roi = true;
    m_ref_roi_lo[0] = cfg.ref_roi_lo().row();
    m_ref_roi_lo[1] = cfg.ref_roi_lo().column();

    m_ref_roi_hi[0] = cfg.ref_roi_hi().row();
    m_ref_roi_hi[1] = cfg.ref_roi_hi().column();
  }
  else {
    m_use_ref_roi = false;
    m_ref_roi_lo[0] = m_ref_roi_hi[0] = 0;
    m_ref_roi_lo[1] = m_ref_roi_hi[1] = 0;
  }

  m_sb_convergence  = cfg.sb_convergence();
  m_ref_convergence = cfg.ref_convergence();

  m_weights = make_ndarray<double>(cfg.number_of_weights());
  std::copy(cfg.weights().begin(), cfg.weights().end(), m_weights.data());

  _indicator_offset = m_use_fit ? 0 : -cfg.number_of_weights()/2;

  m_fit_max_iterations = 0;
  m_fit_weights_factor = 0.;
  m_fit_params = ndarray<double,1>();

  _write_image = cfg.write_image();
  _write_projections = cfg.write_projections();

  unsigned sz = m_projectX ?
    m_sig_roi_hi[1]-m_sig_roi_lo[1]+1 :
    m_sig_roi_hi[0]-m_sig_roi_lo[0]+1;

  if (m_use_fit)
    _fitter->configure(sz,
                       m_fit_max_iterations,
                       m_fit_weights_factor,
                       m_fit_params);

  m_ref_offset = m_use_fit ? 0.0 : 1.0;

  m_flt_offset = m_use_fit ? 0 : m_weights.size() / 2;

  m_ref_avg = load_reference(m_get_key,sz,_ref_path.c_str());
  m_sig = ndarray<const int,1>();
  m_sb  = ndarray<const int,1>();
  m_ref = ndarray<const int,1>();

  m_ref_avg_full = ndarray<double,2>();
  m_sig_full = ndarray<const int,2>();
  m_sb_full  = ndarray<const int,2>();
  m_ref_full = ndarray<const int,2>();

  m_pedestal = 32;

  _cut.clear();
  _cut.resize(NCUTS,0);
}

Fex::Fex(const Pds::Src& src,
         const Pds::TimeTool::ConfigV3& cfg,
         bool write_ref_auto,
         bool verbose,
         const char* ref_path) :
  _ref_path(ref_path ? ref_path : default_ref_path()),
  _write_ref_auto(write_ref_auto),
  _fitter(new Fitter(verbose))
{
  //  m_put_key = std::string(cfg.base_name(),
  //                          cfg.base_name_length());
  m_put_key = std::string(cfg.base_name());

  _src = src;
  m_get_key = src.phy();

  m_beam_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.beam_logic().size());
  std::copy(cfg.beam_logic().begin(),
            cfg.beam_logic().end(),
            m_beam_logic.begin());

  m_laser_logic = make_ndarray<Pds::TimeTool::EventLogic>(cfg.laser_logic().size());
  std::copy(cfg.laser_logic().begin(),
            cfg.laser_logic().end(),
            m_laser_logic.begin());

  m_calib_poly.resize(cfg.calib_poly_dim());
  std::copy(cfg.calib_poly().begin(),cfg.calib_poly().end(),m_calib_poly.data());

  m_projectX = (cfg.project_axis() == Pds::TimeTool::ConfigV3::X);

  m_proj_cut = cfg.signal_cut();

  m_use_full_roi = cfg.use_full_roi();
  m_use_fit = cfg.use_fit();

  m_frame_roi[0] = m_frame_roi[1] = 0;

  m_sig_roi_lo[0] = cfg.sig_roi_lo().row();
  m_sig_roi_lo[1] = cfg.sig_roi_lo().column();

  m_sig_roi_hi[0] = cfg.sig_roi_hi().row();
  m_sig_roi_hi[1] = cfg.sig_roi_hi().column();

  if (cfg.subtract_sideband()) {
    m_use_sb_roi = true;
    m_sb_roi_lo[0] = cfg.sb_roi_lo().row();
    m_sb_roi_lo[1] = cfg.sb_roi_lo().column();

    m_sb_roi_hi[0] = cfg.sb_roi_hi().row();
    m_sb_roi_hi[1] = cfg.sb_roi_hi().column();
  }
  else {
    m_use_sb_roi = false;
    m_sb_roi_lo[0] = m_sb_roi_hi[0] = 0;
    m_sb_roi_lo[1] = m_sb_roi_hi[1] = 0;
  }

  if (cfg.use_reference_roi()) {
    m_use_ref_roi = true;
    m_ref_roi_lo[0] = cfg.ref_roi_lo().row();
    m_ref_roi_lo[1] = cfg.ref_roi_lo().column();

    m_ref_roi_hi[0] = cfg.ref_roi_hi().row();
    m_ref_roi_hi[1] = cfg.ref_roi_hi().column();
  }
  else {
    m_use_ref_roi = false;
    m_ref_roi_lo[0] = m_ref_roi_hi[0] = 0;
    m_ref_roi_lo[1] = m_ref_roi_hi[1] = 0;
  }

  m_sb_convergence  = cfg.sb_convergence();
  m_ref_convergence = cfg.ref_convergence();

  m_weights = make_ndarray<double>(cfg.number_of_weights());
  std::copy(cfg.weights().begin(), cfg.weights().end(), m_weights.data());

  _indicator_offset = m_use_fit ? 0 : -cfg.number_of_weights()/2;

  m_fit_max_iterations = cfg.fit_max_iterations();
  m_fit_weights_factor = cfg.fit_weights_factor();
  m_fit_params = make_ndarray<double>(cfg.fit_params_dim());
  std::copy(cfg.fit_params().begin(), cfg.fit_params().end(), m_fit_params.data());

  _write_image = cfg.write_image();
  _write_projections = cfg.write_projections();

  unsigned col_sz = m_sig_roi_hi[1]-m_sig_roi_lo[1]+1;
  unsigned row_sz = m_sig_roi_hi[0]-m_sig_roi_lo[0]+1;
  unsigned sz = m_projectX ? col_sz : row_sz;

  if (m_use_fit)
    _fitter->configure(sz,
                       m_fit_max_iterations,
                       m_fit_weights_factor,
                       m_fit_params);

  m_ref_offset = m_use_fit ? 0.0 : 1.0;

  m_flt_offset = m_use_fit ? 0 : m_weights.size() / 2;

  const char* dir = _ref_path.c_str();

  m_ref_avg = m_use_full_roi ? ndarray<double,1>() : load_reference(m_get_key,sz,dir);
  m_sig = ndarray<const int,1>();
  m_sb  = ndarray<const int,1>();
  m_ref = ndarray<const int,1>();

  m_ref_avg_full = m_use_full_roi ? load_reference(m_get_key,row_sz,col_sz,dir) : ndarray<double,2>();
  m_sig_full = ndarray<const int,2>();
  m_sb_full  = ndarray<const int,2>();
  m_ref_full = ndarray<const int,2>();

  m_pedestal = 32;

  _cut.clear();
  _cut.resize(NCUTS,0);
}

Fex::~Fex()
{
  unconfigure();
}

void Fex::init_plots()
{
  if (m_use_full_roi)
    _monitor_ref_sig_full( m_ref_avg_full );
  _monitor_ref_sig( m_ref_avg );
}

void Fex::unconfigure()
{
  // Record accumulated reference if auto writing of references is enabled
  char buff[PATH_MAX];
  if (_write_ref_auto) {
    sprintf(buff,"%s/timetool.ref.%08x_bad", _ref_path.c_str(), m_get_key);
    FILE* f = fopen(buff,"w");
    if (f) {
      if (m_use_full_roi) {
        for(unsigned i=0; i<m_ref_avg_full.shape()[0]; i++)
          for(unsigned j=0; j<m_ref_avg_full.shape()[1]; j++)
            fprintf(f," %f",m_ref_avg_full(i,j));
      } else {
        for(unsigned i=0; i<m_ref_avg.size(); i++)
          fprintf(f," %f",m_ref_avg[i]);
      }
      fprintf(f,"\n");
      fclose(f);
    }
  }

  if (_cut[NCALLS]>0) {
    printf("TimeTool::Fex Summary\n");
    for(unsigned i=0; i<NCUTS; i++)
      printf("%s: %3.2f [%u]\n",
             cuts[i],
             double(_cut[i])/double(_cut[NCALLS]),
             _cut[i]);
  }
}

void Fex::configure()
{
  std::string s_exc;

  char buff[PATH_MAX];
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

  m_beam_logic = make_ndarray<Pds::TimeTool::EventLogic>(1);
  { int code = svc.config("event_code_bykik", 162);
    m_beam_logic[0] =
      Pds::TimeTool::EventLogic(abs(code),
                                code>0 ?
                                Pds::TimeTool::EventLogic::L_AND_NOT :
                                Pds::TimeTool::EventLogic::L_AND); }

  m_laser_logic = make_ndarray<Pds::TimeTool::EventLogic>(1);
  { int code = svc.config("event_code_no_laser", 0);
    m_laser_logic[0] =
      Pds::TimeTool::EventLogic(abs(code),
                                code>=0 ?
                                Pds::TimeTool::EventLogic::L_AND_NOT :
                                Pds::TimeTool::EventLogic::L_AND); }

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
        s_exc = std::string("TimeTool: Signal and sideband roi y range sizes differ.");
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
        s_exc = std::string("TimeTool: Signal and sideband roi x range sizes differ.");
      }
    }
    m_use_sb_roi = true;
    m_sb_roi_lo[0] = sb_roi_y[0];
    m_sb_roi_hi[0] = sb_roi_y[1];
    m_sb_roi_lo[1] = sb_roi_x[0];
    m_sb_roi_hi[1] = sb_roi_x[1];
  }
  else {
    m_use_sb_roi = false;
    m_sb_roi_lo[0] = m_sb_roi_hi[0] = 0;
    m_sb_roi_lo[1] = m_sb_roi_hi[1] = 0;
  }

  m_use_ref_roi = false;
  m_ref_roi_lo[0] = m_ref_roi_hi[0] = 0;
  m_ref_roi_lo[1] = m_ref_roi_hi[1] = 0;

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

  m_fit_max_iterations = svc.config("fit_max_iterations", 100);
  m_fit_weights_factor = svc.config("fit_weights_factor", 0.1);

  {
    std::vector<double> fp = svc.config("fit_params",std::vector<double>());
    m_fit_params = make_ndarray<double>(fp.size());
    std::copy(fp.begin(), fp.end(), m_fit_params.data());
  }

  _write_image       = svc.config("write_image",true);
  _write_projections = svc.config("write_projections",false);

  m_use_full_roi = svc.config("use_full_roi",false);
  m_use_fit  = svc.config("use_fit",false);

  unsigned col_sz = m_sig_roi_hi[1]-m_sig_roi_lo[1]+1;
  unsigned row_sz = m_sig_roi_hi[0]-m_sig_roi_lo[0]+1;
  unsigned sz = m_projectX ? col_sz : row_sz;

  if (m_use_fit)
    _fitter->configure(sz,
                       m_fit_max_iterations,
                       m_fit_weights_factor,
                       m_fit_params);

  m_ref_offset = m_use_fit ? 0.0 : 1.0;

  m_flt_offset = m_use_fit ? 0 : m_weights.size() / 2;

  const char* dir = _ref_path.c_str();

  m_ref_avg = m_use_full_roi ? ndarray<double,1>() : load_reference(m_get_key,sz,dir);
  m_sig = ndarray<const int,1>();
  m_sb  = ndarray<const int,1>();
  m_ref = ndarray<const int,1>();

  m_ref_avg_full = m_use_full_roi ? load_reference(m_get_key,row_sz,col_sz,dir) : ndarray<double,2>();
  m_sig_full = ndarray<const int,2>();
  m_sb_full  = ndarray<const int,2>();
  m_ref_full = ndarray<const int,2>();

  m_pedestal = 32;

  _cut.clear();
  _cut.resize(NCUTS,0);

  if (!s_exc.empty())
    throw s_exc;
}

const TimeToolConfigType* Fex::config(const char* fname)
{
  Fex fex(fname);
  fex.configure();
  fex.m_put_key.reserve(fex.m_put_key.size()+1);
  TimeToolConfigType tmplate(fex.m_beam_logic.size(),
                             fex.m_laser_logic.size(),
                             fex.m_weights.size(),
                             fex.m_calib_poly.size(),
                             fex.m_fit_params.size(),
                             fex.m_put_key.size());
  char* p = new char[tmplate._sizeof()];
  return new(p) TimeToolConfigType(fex.m_projectX ? TimeToolConfigType::X : TimeToolConfigType::Y,
                                   fex.m_use_full_roi,
                                   fex.m_use_fit,
                                   fex._write_image,
                                   fex._write_projections,
                                   fex.m_use_sb_roi,
                                   fex.m_use_ref_roi,
                                   fex.m_weights.size(),
                                   fex.m_calib_poly.size(),
                                   fex.m_fit_params.size(),
                                   fex.m_put_key.size(),
                                   fex.m_beam_logic.size(),
                                   fex.m_laser_logic.size(),
                                   fex.m_proj_cut,
                                   fex.m_fit_max_iterations,
                                   fex.m_fit_weights_factor,
                                   Pds::Camera::FrameCoord(fex.m_sig_roi_lo[1],
                                                           fex.m_sig_roi_lo[0]),
                                   Pds::Camera::FrameCoord(fex.m_sig_roi_hi[1],
                                                           fex.m_sig_roi_hi[0]),
                                   Pds::Camera::FrameCoord(fex.m_sb_roi_lo[1],
                                                           fex.m_sb_roi_lo[0]),
                                   Pds::Camera::FrameCoord(fex.m_sb_roi_hi[1],
                                                           fex.m_sb_roi_hi[0]),
                                   fex.m_sb_convergence,
                                   Pds::Camera::FrameCoord(fex.m_ref_roi_lo[1],
                                                           fex.m_ref_roi_lo[0]),
                                   Pds::Camera::FrameCoord(fex.m_ref_roi_hi[1],
                                                           fex.m_ref_roi_hi[0]),
                                   fex.m_ref_convergence,
                                   fex.m_beam_logic.data(),
                                   fex.m_laser_logic.data(),
                                   fex.m_weights.data(),
                                   fex.m_calib_poly.data(),
                                   fex.m_fit_params.data(),
                                   fex.m_put_key.c_str());

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

static bool _calculate_logic(const ndarray<const Pds::TimeTool::EventLogic,1>& cfg,
                             const ndarray<const Pds::EvrData::FIFOEvent,1>& event)
{
  bool v = (cfg[0].logic_op() == Pds::TimeTool::EventLogic::L_AND ||
            cfg[0].logic_op() == Pds::TimeTool::EventLogic::L_AND_NOT);
  for(unsigned i=0; i<cfg.size(); i++) {
    bool p=false;
    for(unsigned j=0; j<event.size(); j++)
      if (event[j].eventCode()==cfg[i].event_code()) {
        p=true;
        break;
      }
    switch(cfg[i].logic_op()) {
    case Pds::TimeTool::EventLogic::L_OR:
      v = v||p; break;
    case Pds::TimeTool::EventLogic::L_AND:
      v = v&&p; break;
    case Pds::TimeTool::EventLogic::L_OR_NOT:
      v = v||!p; break;
    case Pds::TimeTool::EventLogic::L_AND_NOT:
      v = v&&!p; break;
    default: break;
    }
  }
  return v;
}

void Fex::analyze(const ndarray<const uint16_t,2>& f,
                  const ndarray<const Pds::EvrData::FIFOEvent,1>& evr,
                  const Pds::Lusi::IpmFexV1* ipm)
{
  _cut[NCALLS]++;

  bool nobeam   = !_calculate_logic(m_beam_logic,
                                    evr);
  bool nolaser  = !_calculate_logic(m_laser_logic,
                                    evr);

#ifdef DBUG
  printf("event_codes: ");
  for(unsigned i=0; i<evr.size(); i++)
    printf("%d ",evr[i].eventCode());
  printf("\n");

  printf("%05x [%08x] beam %c  laser %c\n",
         evr[0].timestampHigh(),
         1<<(evr[0].timestampLow()&0x1f),
         nobeam ? 'F':'T', nolaser ? 'F':'T');
#endif

  if (nolaser) { _cut[NOLASER]++; return; }

  //
  //  Beam is absent if not enough signal on the IPM detector
  //
  if (ipm)
    nobeam |= ipm->sum() < m_ipm_beam_threshold;

  if (!f.size()) { _cut[FRAMESIZE]++; return; }

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
    if (m_ref_roi_hi[i] >= f.shape()[i]) {
      if (m_projectX == (i==0)) {
        std::stringstream s;
        s << "Timetool: reference " << (i==0 ? 'Y':'X') << " upper bound ["
          << m_ref_roi_hi[i] << "] exceeds frame bounds ["
          << f.shape()[i] << "].";
        msg += s.str();
      }
      m_ref_roi_hi[i] = f.shape()[i]-1;
    }
  }
  if (!msg.empty())
    throw msg;

  ndarray<double,1> sigd = ndarray<double,1>();
  ndarray<double,1> refd = ndarray<double,1>();

  ndarray<double,2> sigd_full = ndarray<double,2>();
  ndarray<double,2> refd_full = ndarray<double,2>();

  //
  //  Project signal ROI
  //
  unsigned pdim = m_projectX ? 1:0;
  if (m_use_full_roi) {
    m_sig_full = psalg::roi(f,
                            m_sig_roi_lo,
                            m_sig_roi_hi,
                            m_pedestal);
    //
    //  Calculate sideband correction
    //
    if (m_use_sb_roi)
      m_sb_full = psalg::roi(f,
                             m_sb_roi_lo,
                             m_sb_roi_hi,
                             m_pedestal);

    //
    //  Calculate reference correction
    //
    if (m_use_ref_roi)
      m_ref_full = psalg::roi(f,
                              m_ref_roi_lo,
                              m_ref_roi_hi,
                              m_pedestal);

    sigd_full = make_ndarray<double>(m_sig_full.shape()[0],m_sig_full.shape()[1]);
    refd_full = make_ndarray<double>(m_sig_full.shape()[0],m_sig_full.shape()[1]);
  } else {
    m_sig = psalg::project(f,
                           m_sig_roi_lo,
                           m_sig_roi_hi,
                           m_pedestal, pdim);

    //
    //  Calculate sideband correction
    //
    if (m_use_sb_roi)
      m_sb = psalg::project(f,
                            m_sb_roi_lo,
                            m_sb_roi_hi,
                            m_pedestal, pdim);

    //
    //  Calculate reference correction
    //
    if (m_use_ref_roi)
      m_ref = psalg::project(f,
                             m_ref_roi_lo,
                             m_ref_roi_hi,
                             m_pedestal, pdim);

    sigd = make_ndarray<double>(m_sig.shape()[0]);
    refd = make_ndarray<double>(m_sig.shape()[0]);
  }

  //
  //  Correct projection for common mode found in sideband
  //
  if (m_sb.size()) {
    psalg::rolling_average(m_sb, m_sb_avg, m_sb_convergence);

    ndarray<const double,1> sbc = psalg::commonModeLROE(m_sb, m_sb_avg);

    if (m_use_ref_roi)
      for(unsigned i=0; i<m_sig.shape()[0]; i++) {
        sigd[i] = double(m_sig[i])-sbc[i];
        refd[i] = double(m_ref[i])-sbc[i];
      }
    else
      for(unsigned i=0; i<m_sig.shape()[0]; i++)
        sigd[i] = double(m_sig[i])-sbc[i];
  }
  else {
    if (m_use_ref_roi)
      for(unsigned i=0; i<m_sig.shape()[0]; i++) {
        sigd[i] = double(m_sig[i]);
        refd[i] = double(m_ref[i]);
      }
    else
      for(unsigned i=0; i<m_sig.shape()[0]; i++)
        sigd[i] = double(m_sig[i]);
  }

  //
  //  Correct full signal/reference for common mode found in sideband
  //
  if (m_sb_full.size()) {
    psalg::rolling_average(m_sb_full, m_sb_avg_full, m_sb_convergence);

    ndarray<const double,2> sbc = psalg::commonModeLROE(m_sb_full, m_sb_avg_full);

    if (m_use_ref_roi)
      for(unsigned i=0; i<m_sig_full.shape()[0]; i++) {
        for(unsigned j=0; j<m_sig_full.shape()[1]; j++) {
          sigd_full(i,j) = double(m_sig_full(i,j))-sbc(i,j);
          refd_full(i,j) = double(m_ref_full(i,j))-sbc(i,j);
        }
      }
    else
      for(unsigned i=0; i<m_sig_full.shape()[0]; i++)
        for(unsigned j=0; j<m_sig_full.shape()[1]; j++)
          sigd_full(i,j) = double(m_sig_full(i,j))-sbc(i,j);
  }
  else {
    if (m_use_ref_roi)
      for(unsigned i=0; i<m_sig_full.shape()[0]; i++) {
        for(unsigned j=0; j<m_sig_full.shape()[1]; j++) {
          sigd_full(i,j) = double(m_sig_full(i,j));
          refd_full(i,j) = double(m_ref_full(i,j));
        }
      }
    else
      for(unsigned i=0; i<m_sig_full.shape()[0]; i++)
        for(unsigned j=0; j<m_sig_full.shape()[1]; j++)
          sigd_full(i,j) = double(m_sig_full(i,j));
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  if (m_use_full_roi) {
    for(unsigned i=0; i<sigd_full.shape()[0]; i++)
      for(unsigned j=0; j<sigd_full.shape()[1]; j++)
        if (sigd_full(i,j)>m_proj_cut)
          lcut=false;
  } else {
    for(unsigned i=0; i<sigd.shape()[0]; i++)
      if (sigd[i]>m_proj_cut)
        lcut=false;
  }

  if (lcut) { _cut[PROJCUT]++; return; }

  //
  //  create projections for sig and ref if using non-projected
  //
  if (m_use_full_roi) {
    sigd = psalg::project(sigd_full, 0.0, pdim);

    if (m_use_ref_roi)
      refd = psalg::project(refd_full, 0.0, pdim);
  }

  if (nobeam) {
    _monitor_ref_sig( m_use_ref_roi ? refd:sigd );
    if (m_use_full_roi) {
      _monitor_ref_sig_full( m_use_ref_roi ? refd_full:sigd_full );
      psalg::rolling_average(ndarray<const double,2>(m_use_ref_roi ? refd_full:sigd_full),
                             m_ref_avg_full, m_ref_convergence);
    } else {
      psalg::rolling_average(ndarray<const double,1>(m_use_ref_roi ? refd:sigd),
                             m_ref_avg, m_ref_convergence);
    }
    _cut[NOBEAM]++;
    return;
  }
  else if (m_use_ref_roi) {
    _monitor_ref_sig( refd );
    if (m_use_full_roi) {
      _monitor_ref_sig_full( refd_full );
      psalg::rolling_average(ndarray<const double,2>(refd_full),
                             m_ref_avg_full, m_ref_convergence);
    } else {
      psalg::rolling_average(ndarray<const double,1>(refd),
                             m_ref_avg, m_ref_convergence);
    }
  }

  _monitor_raw_sig( sigd );
  if (m_use_full_roi)
    _monitor_raw_sig_full( sigd_full );

  if ((m_use_full_roi ? m_ref_avg_full.size() : m_ref_avg.size()) == 0) {
    _cut[NOREF]++;
    return;
  } else {
  }

  //
  //  Divide by the reference
  //
  if (m_use_full_roi) {
    for(unsigned i=0; i<sigd_full.shape()[0]; i++)
      for(unsigned j=0; j<sigd_full.shape()[1]; j++)
        sigd_full(i,j) = sigd_full(i,j)/m_ref_avg_full(i,j) - m_ref_offset;
    _monitor_sub_sig_full( sigd_full );
    // update the signal projection a final time
    sigd = psalg::project(sigd_full, 0.0, pdim);
  } else {
    for(unsigned i=0; i<sigd.shape()[0]; i++)
      sigd[i] = sigd[i]/m_ref_avg[i] - m_ref_offset;
  }

  _monitor_sub_sig( sigd );

  if (m_use_fit) {
    double chisq = 0.;
    ndarray<double,1> params = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> errors = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> qwf = make_ndarray<double>(sigd.size());

    bool converged = _fitter->fit(sigd, params, errors, chisq);

    for (unsigned i=0; i<qwf.size(); i++) {
      qwf[i] = Fitter::erf(i, params[0], params[1], params[2], params[3]);
    }

    _monitor_flt_sig( qwf );
    if (converged) {
      double xflt = params[2]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = params[0];
      _flt_position = xflt;
      _flt_position_ps = xfltc;
      _flt_fwhm = params[1];
      _ref_amplitude = params[3];
      _nxt_amplitude = chisq;
    } else {
      _cut[NOFITS]++;
    }
  } else {
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
        double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

        double  xfltc = 0;
        for(unsigned i=m_calib_poly.size(); i!=0; )
          xfltc = xfltc*xflt + m_calib_poly[--i];

        _amplitude = pFit0[0];
        _flt_position  = xflt;
        _flt_position_ps  = xfltc;
        _flt_fwhm      = pFit0[2];
        _ref_amplitude = m_use_full_roi ?
          psalg::project(m_ref_avg_full,0.0,pdim)[ix] :
          m_ref_avg[ix];

        if (nfits>1) {
          ndarray<double,1> pFit1 =
            psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
          if (pFit1[2]>0)
            _nxt_amplitude = pFit1[0];
        }
      }
    }
    else
      _cut[NOFITS]++;
  }
}

void Fex::analyze(EventType etype,
                  const ndarray<const int,1>& signal,
                  const ndarray<const int,1>& sideband)
{
  _cut[NCALLS]++;

  bool nolaser   = (etype == Dark);

  bool nobeam    = (etype == Reference);

#ifdef DBUG
  printf("beam %c  laser %c\n",
         nobeam ? 'F':'T', nolaser ? 'F':'T');
#endif

  if (nolaser) { _cut[NOLASER]++; return; }

  if (!signal.size()) { _cut[FRAMESIZE]++; return; }

  unsigned pdim = m_projectX ? 1:0;
  m_sig = signal;
  m_sb  = sideband;

  ndarray<double,1> sigd = make_ndarray<double>(m_sig.shape()[0]);

  //
  //  Correct projection for common mode found in sideband
  //
  if (m_sb.size()) {
    psalg::rolling_average(m_sb, m_sb_avg, m_sb_convergence);

    ndarray<const double,1> sbc = psalg::commonModeLROE(m_sb, m_sb_avg);

    for(unsigned i=0; i<m_sig.shape()[0]; i++)
      sigd[i] = double(m_sig[i])-sbc[i];
  }
  else {
    for(unsigned i=0; i<m_sig.shape()[0]; i++)
      sigd[i] = double(m_sig[i]);
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  for(unsigned i=0; i<sigd.shape()[0]; i++)
    if (sigd[i]>m_proj_cut)
      lcut=false;

  if (lcut) { _cut[PROJCUT]++; return; }

  if (nobeam) {
    _monitor_ref_sig( sigd );
    psalg::rolling_average(ndarray<const double,1>(sigd),
                           m_ref_avg, m_ref_convergence);
    _cut[NOBEAM]++;
    return;
  }

  _monitor_raw_sig( sigd );

  if (m_ref_avg.size()==0) {
    _cut[NOREF]++;
    return;
  }

  //
  //  Divide by the reference
  //
  for(unsigned i=0; i<sigd.shape()[0]; i++)
    sigd[i] = sigd[i]/m_ref_avg[i] - m_ref_offset;

  _monitor_sub_sig( sigd );

  if (m_use_fit) {
    double chisq = 0.;
    ndarray<double,1> params = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> errors = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> qwf = make_ndarray<double>(sigd.size());

    bool converged = _fitter->fit(sigd, params, errors, chisq);

    for (unsigned i=0; i<qwf.size(); i++) {
      qwf[i] = Fitter::erf(i, params[0], params[1], params[2], params[3]);
    }

    _monitor_flt_sig( qwf );
    if (converged) {
      double xflt = params[2]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = params[0];
      _flt_position = xflt;
      _flt_position_ps = xfltc;
      _flt_fwhm = params[1];
      _ref_amplitude = params[3];
      _nxt_amplitude = chisq;
    } else {
      _cut[NOFITS]++;
    }
  } else {
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
        double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

        double  xfltc = 0;
        for(unsigned i=m_calib_poly.size(); i!=0; )
          xfltc = xfltc*xflt + m_calib_poly[--i];

        _amplitude = pFit0[0];
        _flt_position  = xflt;
        _flt_position_ps  = xfltc;
        _flt_fwhm      = pFit0[2];
        _ref_amplitude = m_ref_avg[ix];

        if (nfits>1) {
          ndarray<double,1> pFit1 =
            psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
          if (pFit1[2]>0)
            _nxt_amplitude = pFit1[0];
        }
      }
    }
    else
      _cut[NOFITS]++;
  }
}

void Fex::analyze(EventType etype,
                  const ndarray<const int,2>& signal,
                  const ndarray<const int,2>& sideband)
{
  _cut[NCALLS]++;

  bool nolaser   = (etype == Dark);

  bool nobeam    = (etype == Reference);

#ifdef DBUG
  printf("beam %c  laser %c\n",
         nobeam ? 'F':'T', nolaser ? 'F':'T');
#endif

  if (nolaser) { _cut[NOLASER]++; return; }

  if (!signal.size()) { _cut[FRAMESIZE]++; return; }

  unsigned pdim = m_projectX ? 1:0;
  m_sig_full = signal;
  m_sb_full  = sideband;

  ndarray<double,2> sigd_full = make_ndarray<double>(m_sig_full.shape()[0],m_sig_full.shape()[1]);

  //
  //  Correct projection for common mode found in sideband
  //
  if (m_sb.size()) {
    psalg::rolling_average(m_sb_full, m_sb_avg_full, m_sb_convergence);

    ndarray<const double,2> sbc = psalg::commonModeLROE(m_sb_full, m_sb_avg_full);

    for(unsigned i=0; i<m_sig_full.shape()[0]; i++)
      for(unsigned j=0; j<m_sig_full.shape()[1]; j++)
        sigd_full(i,j) = double(m_sig_full(i,j))-sbc(i,j);
  }
  else {
    for(unsigned i=0; i<m_sig_full.shape()[0]; i++)
      for(unsigned j=0; j<m_sig_full.shape()[1]; j++)
        sigd_full(i,j) = double(m_sig_full(i,j));
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  for(unsigned i=0; i<sigd_full.shape()[0]; i++)
    for(unsigned j=0; j<m_sig_full.shape()[1]; j++)
      if (sigd_full(i,j)>m_proj_cut)
        lcut=false;

  if (lcut) { _cut[PROJCUT]++; return; }

  ndarray<double,1> sigd = psalg::project(sigd_full, 0.0, pdim);

  if (nobeam) {
    _monitor_ref_sig( sigd );
    _monitor_ref_sig_full( sigd_full );
    psalg::rolling_average(ndarray<const double,2>(sigd_full),
                           m_ref_avg_full, m_ref_convergence);
    _cut[NOBEAM]++;
    return;
  }

  _monitor_raw_sig ( sigd );
  _monitor_raw_sig_full( sigd_full );

  if (m_ref_avg_full.size()==0) {
    _cut[NOREF]++;
    return;
  }

  //
  //  Divide by the reference
  //
  for(unsigned i=0; i<sigd_full.shape()[0]; i++)
    for(unsigned j=0; j<sigd_full.shape()[1]; j++)
      sigd_full(i,j) = sigd_full(i,j)/m_ref_avg_full(i,j) - m_ref_offset;

  sigd = psalg::project(sigd_full, 0.0, pdim);

  _monitor_sub_sig( sigd );
  _monitor_sub_sig_full( sigd_full );

  if (m_use_fit) {
    double chisq = 0.;
    ndarray<double,1> params = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> errors = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> qwf = make_ndarray<double>(sigd.size());

    bool converged = _fitter->fit(sigd, params, errors, chisq);

    for (unsigned i=0; i<qwf.size(); i++) {
      qwf[i] = Fitter::erf(i, params[0], params[1], params[2], params[3]);
    }

    _monitor_flt_sig( qwf );
    if (converged) {
      double xflt = params[2]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = params[0];
      _flt_position = xflt;
      _flt_position_ps = xfltc;
      _flt_fwhm = params[1];
      _ref_amplitude = params[3];
      _nxt_amplitude = chisq;
    } else {
      _cut[NOFITS]++;
    }
  } else {
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
        double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

        double  xfltc = 0;
        for(unsigned i=m_calib_poly.size(); i!=0; )
          xfltc = xfltc*xflt + m_calib_poly[--i];

        _amplitude = pFit0[0];
        _flt_position  = xflt;
        _flt_position_ps  = xfltc;
        _flt_fwhm      = pFit0[2];
        _ref_amplitude = psalg::project(m_ref_avg_full,0.0,pdim)[ix];

        if (nfits>1) {
          ndarray<double,1> pFit1 =
            psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
          if (pFit1[2]>0)
            _nxt_amplitude = pFit1[0];
        }
      }
    }
    else
      _cut[NOFITS]++;
  }
}

void Fex::analyze(EventType etype,
                  const ndarray<const int,1>& signal,
                  const ndarray<const int,1>& sideband,
                  const ndarray<const int,1>& reference)
{
  _cut[NCALLS]++;

  bool nolaser   = (etype == Dark);

  bool nobeam    = (etype == Reference);

#ifdef DBUG
  printf("beam %c  laser %c\n",
         nobeam ? 'F':'T', nolaser ? 'F':'T');
#endif

  if (nolaser) { _cut[NOLASER]++; return; }

  if (!signal.size()) { _cut[FRAMESIZE]++; return; }

  unsigned pdim = m_projectX ? 1:0;
  m_sig = signal;
  m_sb  = sideband;
  m_ref = reference;

  ndarray<double,1> sigd = make_ndarray<double>(m_sig.shape()[0]);
  ndarray<double,1> refd = make_ndarray<double>(m_sig.shape()[0]);

  //
  //  Correct projection for common mode found in sideband
  //
  if (m_sb.size()) {
    psalg::rolling_average(m_sb, m_sb_avg, m_sb_convergence);

    ndarray<const double,1> sbc = psalg::commonModeLROE(m_sb, m_sb_avg);

    for(unsigned i=0; i<m_sig.shape()[0]; i++) {
      sigd[i] = double(m_sig[i])-sbc[i];
      refd[i] = double(m_ref[i])-sbc[i];
    }
  }
  else {
    for(unsigned i=0; i<m_sig.shape()[0]; i++) {
      sigd[i] = double(m_sig[i]);
      refd[i] = double(m_sig[i]);
    }
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  for(unsigned i=0; i<sigd.shape()[0]; i++)
    if (sigd[i]>m_proj_cut)
      lcut=false;

  if (lcut) { _cut[PROJCUT]++; return; }

  if (nobeam) {
    _monitor_ref_sig( refd );
    psalg::rolling_average(ndarray<const double,1>(refd),
                           m_ref_avg, m_ref_convergence);
    _cut[NOBEAM]++;
    return;
  }

  _monitor_raw_sig( sigd );

  if (m_ref_avg.size()==0) {
    _cut[NOREF]++;
    return;
  }

  //
  //  Divide by the reference
  //
  for(unsigned i=0; i<sigd.shape()[0]; i++)
    sigd[i] = sigd[i]/m_ref_avg[i] - m_ref_offset;

  _monitor_sub_sig( sigd );

  if (m_use_fit) {
    double chisq = 0.;
    ndarray<double,1> params = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> errors = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> qwf = make_ndarray<double>(sigd.size());

    bool converged = _fitter->fit(sigd, params, errors, chisq);

    for (unsigned i=0; i<qwf.size(); i++) {
      qwf[i] = Fitter::erf(i, params[0], params[1], params[2], params[3]);
    }

    _monitor_flt_sig( qwf );
    if (converged) {
      double xflt = params[2]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = params[0];
      _flt_position = xflt;
      _flt_position_ps = xfltc;
      _flt_fwhm = params[1];
      _ref_amplitude = params[3];
      _nxt_amplitude = chisq;
    } else {
      _cut[NOFITS]++;
    }
  } else {
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
        double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

        double  xfltc = 0;
        for(unsigned i=m_calib_poly.size(); i!=0; )
          xfltc = xfltc*xflt + m_calib_poly[--i];

        _amplitude = pFit0[0];
        _flt_position  = xflt;
        _flt_position_ps  = xfltc;
        _flt_fwhm      = pFit0[2];
        _ref_amplitude = m_ref_avg[ix];

        if (nfits>1) {
          ndarray<double,1> pFit1 =
            psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
          if (pFit1[2]>0)
            _nxt_amplitude = pFit1[0];
        }
      }
    }
    else
      _cut[NOFITS]++;
  }
}

void Fex::analyze(EventType etype,
                  const ndarray<const int,2>& signal,
                  const ndarray<const int,2>& sideband,
                  const ndarray<const int,2>& reference)
{
  _cut[NCALLS]++;

  bool nolaser   = (etype == Dark);

  bool nobeam    = (etype == Reference);

#ifdef DBUG
  printf("beam %c  laser %c\n",
         nobeam ? 'F':'T', nolaser ? 'F':'T');
#endif

  if (nolaser) { _cut[NOLASER]++; return; }

  if (!signal.size()) { _cut[FRAMESIZE]++; return; }

  unsigned pdim = m_projectX ? 1:0;
  m_sig_full = signal;
  m_sb_full  = sideband;
  m_ref_full = reference;

  ndarray<double,2> sigd_full = make_ndarray<double>(m_sig_full.shape()[0],m_sig_full.shape()[1]);
  ndarray<double,2> refd_full = make_ndarray<double>(m_sig_full.shape()[0],m_sig_full.shape()[1]);

  //
  //  Correct projection for common mode found in sideband
  //
  if (m_sb_full.size()) {
    psalg::rolling_average(m_sb_full, m_sb_avg_full, m_sb_convergence);

    ndarray<const double,2> sbc = psalg::commonModeLROE(m_sb_full, m_sb_avg_full);

    for(unsigned i=0; i<m_sig_full.shape()[0]; i++) {
      for(unsigned j=0; j<m_sig_full.shape()[1]; j++) {
        sigd_full(i,j) = double(m_sig_full(i,j))-sbc(i,j);
        refd_full(i,j) = double(m_ref_full(i,j))-sbc(i,j);
      }
    }
  }
  else {
    for(unsigned i=0; i<m_sig_full.shape()[0]; i++) {
      for(unsigned j=0; j<m_sig_full.shape()[1]; j++) {
        sigd_full(i,j) = double(m_sig_full(i,j));
        refd_full(i,j) = double(m_sig_full(i,j));
      }
    }
  }

  //
  //  Require projection has a minimum amplitude (else no laser)
  //
  bool lcut=true;
  for(unsigned i=0; i<sigd_full.shape()[0]; i++) {
    for(unsigned j=0; j<sigd_full.shape()[1]; j++) {
    if (sigd_full(i,j)>m_proj_cut)
      lcut=false;
    }
  }

  if (lcut) { _cut[PROJCUT]++; return; }

  ndarray<double,1> sigd = psalg::project(sigd_full, 0.0, pdim);
  ndarray<double,1> refd = psalg::project(refd_full, 0.0, pdim);

  if (nobeam) {
    _monitor_ref_sig( refd );
    _monitor_ref_sig_full( refd_full );
    psalg::rolling_average(ndarray<const double,2>(refd_full),
                           m_ref_avg_full, m_ref_convergence);
    _cut[NOBEAM]++;
    return;
  }

  _monitor_raw_sig( sigd );
  _monitor_raw_sig_full( sigd_full );

  if (m_ref_avg_full.size()==0) {
    _cut[NOREF]++;
    return;
  }

  //
  //  Divide by the reference
  //
  for(unsigned i=0; i<sigd_full.shape()[0]; i++)
    for(unsigned j=0; j<sigd_full.shape()[1]; j++)
      sigd_full(i,j) = sigd_full(i,j)/m_ref_avg_full(i,j) - m_ref_offset;

  sigd = psalg::project(sigd_full, 0.0, pdim);

  _monitor_sub_sig( sigd );
  _monitor_sub_sig_full( sigd_full );

  if (m_use_fit) {
    double chisq = 0.;
    ndarray<double,1> params = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> errors = make_ndarray<double>(Fitter::nparams);
    ndarray<double,1> qwf = make_ndarray<double>(sigd.size());

    bool converged = _fitter->fit(sigd, params, errors, chisq);

    for (unsigned i=0; i<qwf.size(); i++) {
      qwf[i] = Fitter::erf(i, params[0], params[1], params[2], params[3]);
    }

    _monitor_flt_sig( qwf );
    if (converged) {
      double xflt = params[2]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

      double  xfltc = 0;
      for(unsigned i=m_calib_poly.size(); i!=0; )
        xfltc = xfltc*xflt + m_calib_poly[--i];

      _amplitude = params[0];
      _flt_position = xflt;
      _flt_position_ps = xfltc;
      _flt_fwhm = params[1];
      _ref_amplitude = params[3];
      _nxt_amplitude = chisq;
    } else {
      _cut[NOFITS]++;
    }
  } else {
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
        double   xflt = pFit0[1]+m_sig_roi_lo[pdim]+m_frame_roi[pdim]+m_flt_offset;

        double  xfltc = 0;
        for(unsigned i=m_calib_poly.size(); i!=0; )
          xfltc = xfltc*xflt + m_calib_poly[--i];

        _amplitude = pFit0[0];
        _flt_position  = xflt;
        _flt_position_ps  = xfltc;
        _flt_fwhm      = pFit0[2];
        _ref_amplitude = psalg::project(m_ref_avg_full,0.0,pdim)[ix];

        if (nfits>1) {
          ndarray<double,1> pFit1 =
            psalg::parab_fit(qwf,*(++peaks.begin()),0.8);
          if (pFit1[2]>0)
            _nxt_amplitude = pFit1[0];
        }
      }
    }
    else
      _cut[NOFITS]++;
  }
}

ndarray<double,1> load_reference(unsigned key, unsigned sz, const char* dir)
{
  char buff[PATH_MAX];
  ndarray<double,1> m_ref_avg;

  // Load reference
  { sprintf(buff,"%s/timetool.ref.%08x", dir, key);
    std::vector<double> r;
    FILE* rf = fopen(buff,"r");
    if (rf) {
      float rv;
      while( fscanf(rf,"%f",&rv)>0 )
        r.push_back(rv);
      if (r.size()==sz) {
        m_ref_avg = make_ndarray<double>(r.size());
        for(unsigned i=0; i<r.size(); i++)
          m_ref_avg[i] = r[i];
      }
      else {
        printf("Reference in %s size %zu does not match [%u]\n",
               buff, r.size(), sz);
      }
      fclose(rf);
    }
  }
  return m_ref_avg;
}

ndarray<double,2> load_reference(unsigned key, unsigned row_sz, unsigned col_sz, const char* dir)
{
  char buff[PATH_MAX];
  ndarray<double,2> m_ref_avg;

  // Load reference
  { sprintf(buff,"%s/timetool.ref.%08x", dir, key);
    std::vector<double> r;
    FILE* rf = fopen(buff,"r");
    if (rf) {
      float rv;
      while( fscanf(rf,"%f",&rv)>0 )
        r.push_back(rv);
      if (r.size()==(row_sz*col_sz)) {
        m_ref_avg = make_ndarray<double>(row_sz, col_sz);
        for(unsigned i=0; i<row_sz; i++)
          for(unsigned j=0; j<col_sz; j++)
            m_ref_avg(i,j) = r[col_sz*i+j];
      }
      else {
        printf("Reference in %s size %zu does not match [%u]\n",
               buff, r.size(), row_sz*col_sz);
      }
      fclose(rf);
    }
  }
  return m_ref_avg;
}
