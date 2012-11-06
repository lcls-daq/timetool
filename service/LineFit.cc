#include "LineFit.hh"

using namespace TimeTool;

LineFit::LineFit() : _f1(0.05)
{
  reset(); 
}

LineFit::~LineFit() 
{
}

void LineFit::set_convergence(double v) 
{
  _f1=v; 
}

void LineFit::reset() 
{
  v=0; 
  w=0; 
  vv=0; 
  vw=0; 
  ww=0; 
}

void LineFit::accum(double x, double y)
{
  const double f0 = 1-_f1;
  const double f1 = _f1;
  v   =  f0*v  + f1*x;
  w   =  f0*w  + f1*y;
  vv  =  f0*vv + f1*x*x;
  vw  =  f0*vw + f1*x*y;
}

void LineFit::finalize()
{
  double d = vv-v*v;
  if (d==0) {
    _p[0] = _p[1] = 0;
  }
  else {
    _p[1] = ( vw - v*w ) / (vv-v*v);
    _p[0] = w - v*_p[1];
  }
}
