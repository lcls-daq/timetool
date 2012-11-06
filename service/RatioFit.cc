#include "RatioFit.hh"

using namespace TimeTool;

RatioFit::RatioFit() : _fit_p1(false) 
{
}

RatioFit::~RatioFit() 
{
}

void RatioFit::reset() 
{
  vv=0; 
  vw=0;
  xvv=0;
  xvw=0;
  xxvv=0;
  ww=0; 
}

void RatioFit::accum(unsigned k, unsigned cols, double v, double w, double sw)
{
  if (_fit_p1) {
    double q,p;
    double x = double(k)/double(cols) - 0.5;
    double r=1./(sw*sw);
    vv   += (q=v*v*r);
    vw   += (p=v*w*r);
    xvv  += (q*=x);
    xvw  += (p*=x);
    xxvv += (q*=x);
    ww   += w*w*r;
  }
  else {
    vv   += v;
    ww   += w;
  }
}

void RatioFit::finalize()
{
  if (_fit_p1) {
    double d = ( xvv*xvv - xxvv*vv );
    _p[0] = ( xvw*xvv - vw*xxvv ) / d;
    _p[1] = ( vw*xvv  - xvw*vv  ) / d;
    _chsq = ( ww + ( vv*_p[0]*_p[0] + xxvv*_p[1]*_p[1] +2*xvv*_p[0]*_p[1] )
              - 2 * ( vw*_p[0] + xvw*_p[1]) );
  }
  else {
    _p[0] = ww/vv;
    _p[1] = 0;
    _chsq = 0;
  }
}

void   RatioFit::fit_p1(bool v) 
{
  _fit_p1=v; 
}

