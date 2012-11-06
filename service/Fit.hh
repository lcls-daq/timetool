#ifndef TimeTool_Fit_hh
#define TimeTool_Fit_hh

namespace TimeTool {
  class Fit {
    enum { Amplitude, Position, FWHM, NParameters };
  public:
    Fit() {}
    ~Fit() {}
  public:
    void process(double* qwf, double* qwfe, unsigned word, unsigned nwts, unsigned cols);
  public:
    bool   valid    () const { return _valid; }
    double amplitude() const { return _p[Amplitude]; }
    double position () const { return _p[Position ]; }
    double fwhm     () const { return _p[FWHM]; }
  private:
    bool    _valid;
    double  _p[NParameters];
  };
};

#endif
