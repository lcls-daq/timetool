#include "Fex.hh"

#include "pdsdata/camera/FrameV1.hh"
#include "pdsdata/opal1k/ConfigV1.hh"

#include <list>

using namespace TimeTool;

static const int   cols  = Pds::Opal1k::ConfigV1::Column_Pixels;

static void project_spectrumX(const Pds::Camera::FrameV1& frame,
                              unsigned top_row,
                              unsigned bot_row,
                              unsigned lft_col,
                              unsigned rgt_col,
                              uint32_t* sig)
{
  const int width = frame.width();
  const uint16_t* p = reinterpret_cast<const uint16_t*>(frame.data());
  p += top_row*width;
  p += lft_col;
  unsigned i=0;
  while(i<lft_col)
    sig[i++] = 0;
  while(i<=rgt_col)
    sig[i++] = (*p++);
  while(i<(unsigned)width)
    sig[i++] = 0;

  for(unsigned j=top_row+1; j<=bot_row; j++) {
    p = reinterpret_cast<const uint16_t*>(frame.data());
    p += j*width;
    p += lft_col;
    for(unsigned int i=lft_col; i<=rgt_col; i++)
      sig[i] += (*p++);
  }
}

static void project_spectrumY(const Pds::Camera::FrameV1& frame,
                              unsigned lft_col,
                              unsigned rgt_col,
                              unsigned top_row,
                              unsigned bot_row,
                              uint32_t* sig)
{
  const int width = frame.width();
  for(unsigned int i=top_row; i<=bot_row; i++) {
    sig[i] = 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(frame.data());
    p += i*width;
    p += lft_col;
    for(unsigned int j=lft_col; j<=rgt_col; j++)
      sig[i] += (*p++);
  }
}

Fex::Fex(const char* fname) :
  _fname   (fname), 
  _event_code_bykik(162), 
  _event_code_no_laser(0),
  _calib_p0(0),
  _calib_p1(0),
  _calib_p2(0),
  _use_ref (0), 
  _ref_cut (0),
  _sig_cut (0),
  _wts     (0),
  sb       (new uint32_t[cols]),
  ref      (new uint32_t[cols]),
  sig      (new uint32_t[cols]),
  _sb      (cols),
  _sp      (cols),
  _ref     (cols)
{}

Fex::~Fex() 
{
  if (_wts) delete[] _wts; 
  delete[] sb;
  delete[] ref;
  delete[] sig;
}

void Fex::init_plots()
{
  _monitor_ref_sig( _sp.ref() );
}

void Fex::unconfigure()
{
  // Record accumulated reference
  char buff[128];
  const char* dir = getenv("HOME");
  sprintf(buff,"%s/timetool.ref.%08x", dir ? dir : "/tmp", _phy);
  _sp.save_ref(buff);
}

void Fex::configure()
{
  //
  // defaults for absent variables
  //
  _adjust_stats = 0;
  _write_image       = true;
  _write_projections = false;

  char buff[128];
  const char* dir = getenv("HOME");
  sprintf(buff,"%s/%s", dir ? dir : "/tmp", _fname.c_str());
  FILE* f = fopen(buff,"r");
  if (f) {

    printf("TimeTool:Fex reading configuration from file %s\n",buff);

    size_t sz = 8 * 1024;
    char* linep = (char *)malloc(sz);
    char* pEnd;

    while(getline(&linep, &sz, f)>0) {
      if (linep[0]=='#')
        continue;
      const char* delim = " \t\n";
      char* name = strtok(linep,delim);
      char* arg  = strtok(0,delim);
      unsigned v_ul = strtoul(arg,&pEnd,0);
      double   v_db = strtod (arg,&pEnd);
      if      (strcasecmp(name,"base_name")==0)  _base_name= string(arg);
      else if (strcasecmp(name,"phy")==0)        _phy      = v_ul;
      else if (strcasecmp(name,"event_code_bykik")==0)    _event_code_bykik  = v_ul;
      else if (strcasecmp(name,"event_code_no_laser")==0) _event_code_no_laser  = v_ul;
      else if (strcasecmp(name,"calib_p0")==0)    _calib_p0  = v_db;
      else if (strcasecmp(name,"calib_p1")==0)    _calib_p1  = v_db;
      else if (strcasecmp(name,"calib_p2")==0)    _calib_p2  = v_db;
      else if (strcasecmp(name,"use_ref")==0)     _use_ref   = v_ul;
      else if (strcasecmp(name,"ref_fit_p1")==0) _ref_fit_p1  = v_ul;
      else if (strcasecmp(name,"ref_top")==0)    _ref_top  = v_ul;
      else if (strcasecmp(name,"ref_bot")==0)    _ref_bot  = v_ul;
      else if (strcasecmp(name,"ref_cut")==0)    _ref_cut  = v_db;
      else if (strcasecmp(name,"sig_top")==0)    _sig_top  = v_ul;
      else if (strcasecmp(name,"sig_bot")==0)    _sig_bot  = v_ul;
      else if (strcasecmp(name,"sig_cut")==0)    _sig_cut  = v_db;
      else if (strcasecmp(name,"sb_top")==0)     _sb_top   = v_ul;
      else if (strcasecmp(name,"sb_bot")==0)     _sb_bot   = v_ul;
      else if (strcasecmp(name,"spec_begin")==0) _spec_begin = v_ul;
      else if (strcasecmp(name,"spec_end"  )==0) _spec_end   = v_ul;
      else if (strcasecmp(name,"sb_convergence")==0)
        _sb.set_convergence(v_db);
      else if (strcasecmp(name,"sig_convergence")==0)
        _sp.set_convergence(v_db);
      else if (strcasecmp(name,"indicator_offset")==0)
        _indicator_offset = v_db;
      else if (strcasecmp(name,"ref_update_period")==0)
        _ref_corr.set_period(v_ul);
      else if (strcasecmp(name,"nwts")==0) {
        _nwts     = v_ul;
        if (_wts) delete _wts;
        _wts      = new double[_nwts];
        for(int i=0; i<int(_nwts); i++) {
          getline(&linep, &sz, f);
          _wts[i] = strtod(linep,&pEnd);
        }
      }
      else if (strcasecmp(name,"project")==0) {
        _projectX = arg[0]=='x' || arg[0]=='X';
        printf("project %c [%c]\n", _projectX ? 'X' : 'Y',arg[0]);
      }
      else if (strcasecmp(name,"write_image")==0)
	_write_image = arg[0]=='t' || arg[0]=='T';
      else if (strcasecmp(name,"write_projections")==0)
	_write_projections = arg[0]=='t' || arg[0]=='T';
      else if (strcasecmp(name,"adjust_stats")==0)
        _adjust_stats = v_ul;
      else if (strcasecmp(name,"adjust_pv")==0)
        _adjust_pv = string(arg);
      else
        printf("Unknown parameter name %s\n",name);
    }

    free(linep);
        

    fclose(f);

    //        _use_ref = 0;  // hardwire
    _sp .fit_ratio(_use_ref==2); _sp .ratio_fit().fit_p1(_ref_fit_p1);
    _ref.fit_ratio(_use_ref==2); _ref.ratio_fit().fit_p1(_ref_fit_p1);

    _flt_center_ps = _calib_p0 + _calib_p1*double(cols>>1) + _calib_p2*double(cols*cols>>2);

    // Load reference
    char buff[128];
    const char* dir = getenv("HOME");
    sprintf(buff,"%s/timetool.ref.%08x", dir ? dir : "/tmp", _phy);
    _sp.load_ref(buff);

    printf("TT use_ref %u\n",_use_ref);
  }
  else
    printf("Failed to open %s\n",buff);
}

void Fex::reset() 
{
  _nfits=0;
  _amplitude=_raw_position=_flt_position=_flt_position_ps=_flt_fwhm=_nxt_amplitude=_ref_amplitude=0;
}

void Fex::analyze(const Pds::Camera::FrameV1& frame,
                  bool bykik, bool no_laser)
{
  if (_projectX) {
    project_spectrumX(frame, 
                      _sig_top, _sig_bot,
                      _spec_begin, _spec_end,
                      sig);
    project_spectrumX(frame, 
                      _sb_top, _sb_bot,
                      _spec_begin, _spec_end,
                      sb);
    if (_use_ref)
      project_spectrumX(frame, 
                        _ref_top, _ref_bot,
                        _spec_begin, _spec_end,
                        ref);
  }
  else {
    project_spectrumY(frame, 
                      _sig_top, _sig_bot,
                      _spec_begin, _spec_end,
                      sig);
    project_spectrumY(frame, 
                      _sb_top, _sb_bot,
                      _spec_begin, _spec_end,
                      sb);
    if (_use_ref)
      project_spectrumY(frame, 
                        _ref_top, _ref_bot,
                        _spec_begin, _spec_end,
                        ref);
  }

  analyze(sig, sb, ref, bykik, no_laser);
}

void Fex::analyze(const uint32_t* u_sig,
		  const uint32_t* u_sb,
		  const uint32_t* u_ref,
                  bool bykik, bool no_laser)
{
  reset();

  if (no_laser) return;

  double* sbwf = _sb.process(u_sb);

  if (bykik) {
    //  Add ratio fit results to correlation
    if (_use_ref==2) {
      _ref.process_ref(u_ref,sbwf,_ref_cut);
      _sp .process_ref(u_sig,sbwf,_sig_cut);
      _ref_corr.accum (_ref.ratio_fit(),_sp.ratio_fit());
      _monitor_corr   (_ref.ratio_fit(),_sp.ratio_fit());
    }
    else {
      _monitor_ref_sig( _sp .process_ref(u_sig,sbwf,_sig_cut) );
    }
  }
  else {
    _monitor_raw_sig( u_sig, sbwf );

    double* sigwf;
    if (_use_ref==2) {
      //  Use ratio fit correlation to correct spectra changes
      _monitor_ref_sig( _ref.process_ref(u_ref,sbwf,_ref_cut) );
      sigwf = _sp.process_sig(u_sig,sbwf,_sig_cut,
                              _ref_corr.p0(_ref.ratio_fit().p0()),
                              _ref_corr.p1(_ref.ratio_fit().p1()));
    }
    else if (_use_ref==1) {
      //  Use reference trace as a straight substitution
      double* rref = _ref.process_ref(u_ref,sbwf,_ref_cut);
      _monitor_ref_sig( rref );
      sigwf = _sp.process_sig(u_sig,sbwf,_sig_cut,rref);
    }
    else {
      sigwf = _sp.process_sig(u_sig,sbwf,_sig_cut);
    }

    if (!sigwf) return;

    _monitor_sub_sig( sigwf );

    double* qwf  = new double[cols];
    double* qwfe = new double[cols];
    const double noise=25.;

    unsigned x0 = _nwts+_spec_begin;
    for(int i=0; i<cols; i++)
      qwf[i] = 0;
    for(int i=x0; i<=_spec_end; i++) {
      for(int j=0; j<int(_nwts); j++)
        qwf[i] += _wts[j]*sigwf[i-j];
      qwfe[i] = _wtsig*noise;
    }

    _monitor_flt_sig( qwf );

    std::list<int> peaks;

    double amax    = qwf[x0] > 0 ? qwf[x0] : 0;
    double aleft   = amax;
    double aright  = 0;
    unsigned imax  = x0;
    const double afrac = 0.50;

    _nfits = 0;
    bool lpeak = false;

    for(int i=x0+1; i<_spec_end; i++) {
      if (qwf[i] > amax) {
        amax = qwf[i];
        if (amax*afrac > aleft) {
          imax = i;
          lpeak  = true;
          aright = afrac*amax;
        } 
      }
      else if (lpeak && qwf[i] < aright) {
        if (peaks.size()==MaxFits && qwf[peaks.back()&0xffff]>amax)
          ;
        else {
          int word = (i<<16) | imax;
              
          if (peaks.size()==MaxFits)
            peaks.pop_back();

          int sz = peaks.size();
          for(std::list<int>::iterator it=peaks.begin(); it!=peaks.end(); it++)
            if (qwf[(*it)&0xffff]<amax) {
              peaks.insert(it,word);
              break;
            }
          if (sz == int(peaks.size()))
            peaks.push_back(word);
        }

        lpeak = false;
        amax  = aleft = (qwf[i]>0 ? qwf[i] : 0);
      }
      else if (!lpeak && qwf[i] < aleft) {
        amax = aleft = (qwf[i] > 0 ? qwf[i] : 0);
      }
    }

    //      for(std::list<int>::const_iterator it=peaks.begin(); it!=peaks.end(); it++, _nfits++)
    //        _fits[_nfits].process(qwf,qwfe,*it,_nwts);

    _flt_position  = 0;
    _flt_fwhm      = 0;
    _amplitude     = 0;
    _nxt_amplitude = -1;
    _nfits = peaks.size();
    if (_nfits>0) {
      int word = *peaks.begin();
      int ix   = word&0xffff;
      _raw_position = ix;
      _fits[0].process(qwf,qwfe,word,_nwts, cols);
      if (_fits[0].valid()) {
        double x = _fits[0].position();
        _flt_position = x;
        _flt_position_ps   = _calib_p0 + x*_calib_p1 + x*x*_calib_p2;
        _flt_fwhm     = _fits[0].fwhm();
        _amplitude    = _fits[0].amplitude();
        _ref_amplitude= _use_ref ? _ref.ref()[ix] : _sp.ref()[ix];
        _nxt_amplitude = -1;
        if (_nfits>1) {
          _fits[1].process(qwf,qwfe,*(++peaks.begin()),_nwts, cols);
          if (_fits[1].valid())
            _nxt_amplitude = _fits[1].amplitude();
        }
      }
    }
    delete[] qwf;
    delete[] qwfe;
  }
}
