#include "Fit.hh"
#include "pdsalg/pdsalg.h"
#include "ndarray/ndarray.h"

using namespace TimeTool;

void Fit::process(double* qwf, double* qwfe, unsigned word, unsigned nwts, unsigned cols)
{
  unsigned ix = word&0xffff;

  //  Fit upper 20%
  const double afrac = 0.8;

  const double trf = afrac*qwf[ix];
  int ix_left(ix);
  while(--ix_left > int(nwts)) {
    if (qwf[ix_left] < trf)
      break;
  }

  int ix_right(ix);
  while(++ix_right < int(cols)-1) {
    if (qwf[ix_right] < trf)
      break;
  }

  ndarray<const double,1> input = make_ndarray(&qwf[ix_left],ix_right-ix_left+1);
  ndarray<double,1> a = pdsalg::parab_fit(input);

  if (a[2] < 0) {  // a maximum
    _p[Amplitude] = a[0] - 0.25*a[1]*a[1]/a[2];
    _p[Position ] = -0.5*a[1]/a[2];
    _valid = true;
  }

  const double hm = 0.5*qwf[ix];
  while(qwf[ix_left]>hm && ix_left>int(nwts))
    ix_left--;
  while(qwf[ix_right]>hm && ix_right<int(cols)-1)
    ix_right++;

  _p[FWHM] = double(ix_right-ix_left) -
    (hm-qwf[ix_left ])/(qwf[ix_left +1]-qwf[ix_left ]) -
    (hm-qwf[ix_right])/(qwf[ix_right+1]-qwf[ix_right]);
}
