#include "RefCorr.hh"
#include "RatioFit.hh"

using namespace TimeTool;

RefCorr::RefCorr() : _n(10), _i(0) 
{
  //
  //  Initialize with 1:1 normalization correction 
  //
  _p0p[0]=1; _p0p[1]=0; _p0.reset();
  _p1p[0]=0; _p1p[1]=0; _p1.reset();
}

RefCorr::~RefCorr() 
{
}

void RefCorr::set_period(int n) 
{
  _n=n; 
  _i=0; 
}

void RefCorr::accum(const RatioFit& input, const RatioFit& output)
{
  _p0.accum(input.p0(),output.p0());
  _p1.accum(input.p1(),output.p1());
  if (++_i == _n) {
    _i = 0;
    _p0.finalize();
    _p0p[0] = _p0.p0();
    _p0p[1] = _p0.p1();
    _p0.reset();
    _p1.finalize();
    _p1p[0] = _p1.p0();
    _p1p[1] = _p1.p1();
    _p1.reset();
  }
}
