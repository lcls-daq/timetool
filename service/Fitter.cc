#include "Fitter.hh"

#include <gsl/gsl_blas.h>

using namespace TimeTool;

static const unsigned xoffset = 1;
static const double xtol = 1e-8;
static const double gtol = 1e-8;
static const double ftol = 0;

#define FIT(i) gsl_vector_get(_w->x, i)
#define ERR(i) sqrt(gsl_matrix_get(_covar,i,i))

static void step_print(const size_t iter, void *params,
                       const gsl_multifit_nlinear_workspace *w)
{
  gsl_vector *f = gsl_multifit_nlinear_residual(w);
  gsl_vector *x = gsl_multifit_nlinear_position(w);
  double rcond;

  /* compute reciprocal condition number of J(x) */
  gsl_multifit_nlinear_rcond(&rcond, w);

  printf("iter %2zu: a = %.4f, b = %.4f, c = %.4f, d = %.4f, cond(J) = %8.4f, |f(x)| = %.4f\n",
         iter,
         gsl_vector_get(x, 0),
         gsl_vector_get(x, 1),
         gsl_vector_get(x, 2),
         gsl_vector_get(x, 3),
         1.0 / rcond,
         gsl_blas_dnrm2(f));
}

Fitter::Fitter() :
  _verbose(false),
  _maxiter(0),
  _scale(0.),
  _times(NULL),
  _values(NULL),
  _T(gsl_multifit_nlinear_trust),
  _w(NULL),
  _fdf_params(gsl_multifit_nlinear_default_parameters()),
  _covar(gsl_matrix_alloc (nparams, nparams))
{}

Fitter::Fitter(bool verbose) :
  _verbose(verbose),
  _maxiter(0),
  _scale(0.),
  _times(NULL),
  _values(NULL),
  _T(gsl_multifit_nlinear_trust),
  _w(NULL),
  _fdf_params(gsl_multifit_nlinear_default_parameters()),
  _covar(gsl_matrix_alloc (nparams, nparams))
{}

Fitter::~Fitter()
{
  if (_w) gsl_multifit_nlinear_free(_w);
  if (_covar) gsl_matrix_free(_covar);
}

size_t Fitter::npoints() const
{
  return _fdf.n;
}

void Fitter::configure(size_t npoints,
                       size_t maxiter,
                       double scale,
                       const ndarray<const double,1>& fit_params)
{
  /* free old versions of the solver. */
  if (_w) gsl_multifit_nlinear_free(_w);
  if (_times) delete _times;
  if (_values) _values = NULL;

  _maxiter = maxiter;
  _scale = scale;
  /* make a copy of the fit parameters */
  _fit_params = make_ndarray<double>(fit_params.size());
  std::copy(fit_params.begin(), fit_params.end(), _fit_params.begin());

  /* initialize the x values */
  _times = new double[npoints];
  for (unsigned i=0; i<npoints; i++)
    _times[i] = i+xoffset;
  /* initialize the fdf */
  _fdf.f = erf_f;
  _fdf.df = erf_df;
  _fdf.fvv = NULL;
  _fdf.n = npoints;
  _fdf.p = nparams;
  _fdf.params = this;

  /* allocate with size of data and num parameters */
  _w = gsl_multifit_nlinear_alloc (_T, &_fdf_params, npoints, nparams);
}

bool Fitter::fit(const ndarray<const double,1>& input,
                 ndarray<double,1>& params,
                 ndarray<double,1>& errors,
                 double& chisqpdof)
{
  int status, info;
  double chisq0, chisq;
  gsl_vector *f;
  gsl_matrix *J;

  /* sanity check the inputs */
  if (_fit_params.size() != nparams) {
    fprintf(stderr, "Unexpected number of starting fit values: %zu vs %zu\n",
           _fit_params.size(),
           nparams);
    return false;
  } else if(params.size() < nparams) {
    fprintf(stderr, "Parameter output array has insufficient size to hold output!\n");
    return false;
  } else if(errors.size() < nparams) {
    fprintf(stderr, "Parameter errors array has insufficient size to hold output!\n");
    return false;
  } else if(input.size() != _fdf.n) {
    fprintf(stderr, "Unexpected size of input data: %zu vs %zu\n",
            input.size(),
            _fdf.n);
    return false;
  } else if (!_w) {
    fprintf(stderr, "The solver has not been configured!\n");
    return false;
  }

  /* get pointer to input array's data */
  _values = input.data();

  /* create gsl vector view for params */
  gsl_vector_view x = gsl_vector_view_array(_fit_params.data(), nparams);

  /* if the weights scale factor is > 0.0 then create weights to use */
  if (_scale > 0.0) {
    /* create weights */
    ndarray<double,1> weights = make_ndarray<double>(input.size());
    for (unsigned i=0; i<weights.size(); i++)
      weights[i] = _scale * input[i];

    /* create gsl vector view for weights */
    gsl_vector_view wts = gsl_vector_view_array(weights.data(), _fdf.n);

    /* initialize solver with starting values and weights */
    gsl_multifit_nlinear_winit (&x.vector, &wts.vector, &_fdf, _w);
  } else {
    /* initialize solver with starting values without weights */
    gsl_multifit_nlinear_init  (&x.vector, &_fdf, _w);
  }

  /* compute initial chisq */
  f = gsl_multifit_nlinear_residual(_w);
  gsl_blas_ddot(f, f, &chisq0);

  /* run the solver */
  status = gsl_multifit_nlinear_driver(_maxiter, xtol, gtol, ftol,
                                       _verbose ? step_print : NULL,
                                       NULL, &info, _w);

  /* compute covariance */
  J = gsl_multifit_nlinear_jac(_w);
  gsl_multifit_nlinear_covar (J, 0.0, _covar);

  /* compute final chisq */
  gsl_blas_ddot(f, f, &chisq);

  {
    double dof = _fdf.n - nparams;
    double c = GSL_MAX_DBL(1, sqrt(chisq / dof));

    /* fill the outputs */
    chisqpdof = chisq / dof;
    params[0] = FIT(0);         errors[0] = c*ERR(0);
    params[1] = FIT(1);         errors[1] = c*ERR(1);
    params[2] = FIT(2)-xoffset; errors[2] = c*ERR(2);
    params[3] = FIT(3);         errors[3] = c*ERR(3);
  }

  if(_verbose) {
    printf("summary from method '%s/%s'\n",
           gsl_multifit_nlinear_name(_w),
           gsl_multifit_nlinear_trs_name(_w));
    printf("number of iterations: %zu\n",
           gsl_multifit_nlinear_niter(_w));
    printf("function evaluations: %zu\n", _fdf.nevalf);
    printf("Jacobian evaluations: %zu\n", _fdf.nevaldf);
    printf("reason for stopping: %s\n",
            (info == 1) ? "small step size" : "small gradient");
    printf("initial |f(x)| = %f\n", sqrt(chisq0));
    printf("final   |f(x)| = %f\n", sqrt(chisq));

    printf("chisq/dof = %g\n", chisqpdof);

    printf ("a      = %.5f +/- %.5f\n", params[0], errors[0]);
    printf ("b      = %.5f +/- %.5f\n", params[1], errors[1]);
    printf ("c      = %.5f +/- %.5f\n", params[2], errors[2]);
    printf ("d      = %.5f +/- %.5f\n", params[3], errors[3]);

    printf ("status = %s\n", gsl_strerror (status));
  }

  return status == GSL_SUCCESS;
}

double Fitter::erf(double x, double a, double b, double c, double d)
{
  return d + (a - d) / (1. + pow(x / c, b));
}

int Fitter::erf_f(const gsl_vector* x, void* data, gsl_vector* f)
{
  Fitter* fitter = reinterpret_cast<Fitter*>(data);
  size_t n = fitter->_fdf.n;
  double* t = fitter->_times;
  const double* y = fitter->_values;

  double a = gsl_vector_get (x, 0);
  double b = gsl_vector_get (x, 1);
  double c = gsl_vector_get (x, 2);
  double d = gsl_vector_get (x, 3);

  for (size_t i=0; i<n; i++) {
    double Yi = Fitter::erf(t[i], a, b, c, d);
    gsl_vector_set (f, i, Yi - y[i]);
  }

  return GSL_SUCCESS;
}

int Fitter::erf_df(const gsl_vector* x, void* data, gsl_matrix* J)
{
  Fitter* fitter = reinterpret_cast<Fitter*>(data);
  size_t n = fitter->_fdf.n;
  double* t = fitter->_times;

  double a = gsl_vector_get (x, 0);
  double b = gsl_vector_get (x, 1);
  double c = gsl_vector_get (x, 2);
  double d = gsl_vector_get (x, 3);

  for (size_t i = 0; i < n; i++)
    {
      /* Jacobian matrix J(i,j) = dfi / dxj, */
      double e = t[i] / c;
      double f = pow(e, b);
      double g = pow(f + 1., 2.);
      gsl_matrix_set (J, i, 0, 1. / (1. + f));
      gsl_matrix_set (J, i, 1, (d-a) * f * log(e) / g);
      gsl_matrix_set (J, i, 2, (b * (a -d) * f) / (g * c));
      gsl_matrix_set (J, i, 3, 1. - (1. /(f + 1.)));
    }

  return GSL_SUCCESS;
}

#undef FIR
#undef ERR
