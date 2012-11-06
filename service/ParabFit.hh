#ifndef TimeTool_ParabFit_hh
#define TimeTool_ParabFit_hh

namespace TimeTool {
  class ParabFit {
  public:
    ParabFit();
    ~ParabFit();
  public:
    void accum(int ix, double y);
    void finalize();
  public:
    bool   valid    () const { return _valid; }
    double amplitude() const { return _a - 0.25*_b*_b/_c; }
    double position () const { return -0.5*_b/_c; }
    double position_error() const { return 0; }
    double chisquare() const { return 0; }
  private:
    bool   _valid;
    double xx[5], xy[3];
    double _a, _b, _c;
  };
};

#endif
