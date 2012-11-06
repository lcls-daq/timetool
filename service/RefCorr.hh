#ifndef TimeTool_RefCorr_hh
#define TimeTool_RefCorr_hh

#include "LineFit.hh"

namespace TimeTool {
  class RatioFit;
  class RefCorr {
  public:
    RefCorr();
    ~RefCorr();
  public:
    void set_period(int n);
    void accum(const RatioFit& input, const RatioFit& output);
  public:
    double p0(double x) const { return _p0p[0] + x*_p0p[1]; }
    double p1(double x) const { return _p1p[0] + x*_p1p[1]; }
  private:
    unsigned _n, _i;
    LineFit _p0;
    LineFit _p1;
    double  _p0p[2];
    double  _p1p[2];
  };
};

#endif
