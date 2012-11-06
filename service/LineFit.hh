#ifndef TimeTool_LineFit_hh
#define TimeTool_LineFit_hh

namespace TimeTool {

  class LineFit {
  public:
    LineFit();
    ~LineFit();
  public:
    void set_convergence(double v);
    void reset();
    void accum(double x, double y);
    void finalize();
  public:
    double p0  () const { return _p[0]; }
    double p1  () const { return _p[1]; }
    double chsq() const { return _chsq; }
  private:
    double _f1;
    double v, w, vv, vw, ww;
    double _p[2];
    double _chsq;
  };
};

#endif
