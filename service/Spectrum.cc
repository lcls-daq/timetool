#include "Spectrum.hh"

#include <stdlib.h>
#include <stdio.h>

using namespace TimeTool;

Spectrum::Spectrum(unsigned cols) :
  _cols(cols),
  _wf  (new double[cols]),
  _ref (new double[cols]),
  _f1  (0.25),
  _init(false),
  _fit_ratio(false)
{
  for(unsigned k=0; k<cols; k++) _ref[k]=1.;
}

Spectrum::~Spectrum()
{
  delete[] _wf;
  delete[] _ref;
}

double* Spectrum::process_ref(const uint32_t* wf, double* sb, double cut)
{	
  bool lcut=true;
  int  cols=int(_cols);

  for(int k=0; k<cols; k++)
    if ((_wf[k] = double(wf[k]) - sb[k])>cut)
      lcut=false;

  if (lcut) return _ref;

  if (!_init) {
    _init=true;
    for(int k=0; k<cols; k++)
      _ref[k] = _wf[k];
  }
  else if (_fit_ratio) {
    //
    //  (1) Fit a 1d poly to the ratio of this frame to the reference
    //  (2) Update the reference
    //
    _ratio_fit.reset();
    const double f0 = 1-_f1;
    const double f1 = _f1;
    const double _noise = 25;
    for(int k=0; k<cols; k++) {
      _ratio_fit.accum(k, _cols, _ref[k], _wf[k], _noise);  // (1)
      _ref[k] = f0*_ref[k] + f1*_wf[k];              // (2)
    }
    _ratio_fit.finalize();
  }
  else {
    const double f0 = 1-_f1;
    const double f1 = _f1;
    for(int k=0; k<cols; k++)
      _ref[k] = f0*_ref[k] + f1*_wf[k];
  }
  return _ref;	
}

double* Spectrum::process_sig(const uint32_t* wf, double* sb, double cut)
{
  bool lcut=true;
  for(unsigned k=0; k<_cols; k++) {
    double v = double(wf[k]) - sb[k];
    if (v > cut) { 
      lcut=false;
      _wf[k] = v/_ref[k] - 1;
    }
    else
      _wf[k] = 0;
  }
  return lcut ? 0 : _wf;
}

double* Spectrum::process_sig(const uint32_t* wf, double* sb, double cut, double* ref)
{
  bool lcut=true;
  for(unsigned k=0; k<_cols; k++) {
    double v = double(wf[k]) - sb[k];
    if (v > cut) {
      lcut=false;
      _wf[k] = v/ref[k] - 1;
    }
    else
      _wf[k] = 0;
  }
  return lcut ? 0 : _wf;
}

double* Spectrum::process_sig(const uint32_t* wf, double* sb, double cut,
                              double p0, double p1)
{
  bool lcut=true;
  for(unsigned k=0; k<_cols; k++) {
    double r = p0+p1*(double(k)/double(_cols)-0.5);
    double v = double(wf[k]) - sb[k];
    if (v > cut) lcut=false;
    _wf[k] = v/(_ref[k]*r) - 1;
  }
  return lcut ? 0 : _wf;
}

void Spectrum::save_ref(const char* fname) {
  if (_init) {
    FILE* f = fopen(fname,"w");
    if (f) {
      printf("Saving reference to %s\n",fname);
      for(unsigned k=0; k<_cols; k++) 
        fprintf(f,"%g\n",_ref[k]);
      fclose(f);
    }
    else {
      printf("Failed to save reference to %s\n",fname);
    }
  }
  else
    printf("No reference to save to %s\n",fname);
}

void Spectrum::load_ref(const char* fname) {
  FILE* f = fopen(fname,"r");
  if (f) {
    printf("Loading reference from %s\n",fname);
    _init = true;
    float v;
    for(unsigned k=0; k<_cols; k++) {
      fscanf(f,"%g",&v);	
      _ref[k] = v;	
    }
    fclose(f);
  }
  else
    printf("No reference file %s\n",fname);
}

void Spectrum::set_convergence(double v) { _f1=v; }

void      Spectrum::fit_ratio(bool v) { _fit_ratio = v; }
