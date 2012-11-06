#ifndef TimeTool_RatioFit_hh
#define TimeTool_RatioFit_hh

namespace TimeTool {
  class RatioFit {
  public:
    RatioFit();
    ~RatioFit();
  public:
    void reset();
    void accum(unsigned k, unsigned cols, double v, double w, double sw);
    void finalize();
  public:
    void   fit_p1(bool v);
  public:
    double p0  () const { return _p[0]; }
    double p1  () const { return _p[1]; }
    double chsq() const { return _chsq; }
  private:
    double vv, vw, xvv, xvw, xxvv, ww;
    double _p[2];
    double _chsq;
    bool   _fit_p1;
  };
};

#endif
