#ifndef TimeTool_Spectrum_hh
#define TimeTool_Spectrum_hh

#include "RatioFit.hh"

#include <stdint.h>

namespace TimeTool {
  class Spectrum {
  public:
    Spectrum(unsigned);
    ~Spectrum();
  public:
    double* process_ref(const uint32_t* wf, double* sb, double cut);
    double* process_sig(const uint32_t* wf, double* sb, double cut);
    double* process_sig(const uint32_t* wf, double* sb, double cut, double* ref);
    double* process_sig(const uint32_t* wf, double* sb, double cut,
                        double p0, double p1);
    void save_ref(const char* fname);
    void load_ref(const char* fname);
    void set_convergence(double v);
  public:
    void      fit_ratio(bool v);
    RatioFit& ratio_fit()       { return _ratio_fit; }
    const double* ref  () const { return _ref; }
  protected:
    unsigned        _cols;
    double*         _raw;
    double*         _wf;
    double*         _ref;
    double          _f1;
    bool            _init;
    bool            _fit_ratio;
    RatioFit        _ratio_fit;
  };
};

#endif
