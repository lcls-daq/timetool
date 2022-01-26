#ifndef TimeTool_Fitter_hh
#define TimeTool_Fitter_hh

#include "ndarray/ndarray.h"

#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_multifit_nlinear.h>

namespace TimeTool {

  class Fitter {
  public:
    Fitter();
    Fitter(bool verbose);
    virtual ~Fitter();

    size_t npoints() const;
    void configure(size_t npoints,
                   size_t maxiter,
                   double scale,
                   const ndarray<const double,1>& fit_params);

    bool fit(const ndarray<const double,1>& input,
             ndarray<double,1>& params,
             ndarray<double,1>& errors,
             double& chisqpdof);

    static double erf(double x, double a, double b, double c, double d);
    static int erf_f(const gsl_vector* x, void* data, gsl_vector* f);
    static int erf_df(const gsl_vector* x, void* data, gsl_matrix* J);
    static const size_t nparams = 4;
  private:
    bool                             _verbose;
    size_t                           _maxiter;
    double                           _scale;
    double*                          _times;
    const double*                    _values;
    ndarray<double,1>                _fit_params;
    const gsl_multifit_nlinear_type* _T;
    gsl_multifit_nlinear_workspace*  _w;
    gsl_multifit_nlinear_fdf         _fdf;
    gsl_multifit_nlinear_parameters  _fdf_params;
    gsl_matrix*                      _covar;
  };
}

#endif
