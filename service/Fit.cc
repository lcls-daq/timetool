#include "Fit.hh"
#include "ParabFit.hh"

using namespace TimeTool;

void Fit::process(double* qwf, double* qwfe, unsigned word, unsigned nwts, unsigned cols)
{
  unsigned ix = word&0xffff;

  //  Fit parabola
  ParabFit pfit;
  pfit.accum(ix,qwf[ix]);

  //  Fit upper 20%
  const double afrac = 0.8;

  const double trf = afrac*qwf[ix];
  int ix_left(ix);
  while(--ix_left > int(nwts)) {
    pfit.accum(ix_left,qwf[ix_left]);
    if (qwf[ix_left] < trf)
      break;
  }

  int ix_right(ix);
  while(++ix_right < int(cols)-1) {
    pfit.accum(ix_right,qwf[ix_right]);
    if (qwf[ix_right] < trf)
      break;
  }
    
  pfit.finalize();

  if (pfit.valid()) {
    _p[Amplitude] = pfit.amplitude();
    _p[Position ] = pfit.position();
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
