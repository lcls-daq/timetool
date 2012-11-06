#include "ParabFit.hh"

#include <string.h>
#include <stdio.h>

using namespace TimeTool;

static bool verbose=false;

ParabFit::ParabFit()
{
  memset(xx,0,5*sizeof(double));
  memset(xy,0,3*sizeof(double));
}

ParabFit::~ParabFit() {}

void ParabFit::accum(int ix, double y)
{
  double x = double(ix);
  double qx =x;
  xx[0]++;
  xy[0] += y;
  xx[1] += x;
  xy[1] += (y*=x);
  xx[2] += (qx*=x);
  xy[2] += y*x;
  xx[3] += (qx*=x);
  xx[4] += qx*x;
}

void ParabFit::finalize()
{
  _valid = false;

  double a11 = xx[0];
  double a21 = xx[1];
  double a31 = xx[2];
  double a22 = xx[2];
  double a32 = xx[3];
  double a33 = xx[4];

  double b11 = a22*a33-a32*a32;
  double b21 = a21*a33-a32*a31;
  double b31 = a21*a32-a31*a22;
  double b22 = a11*a33-a31*a31;
  double b32 = a11*a32-a21*a31;
  double b33 = a11*a22-a21*a21;

  double det = a11*b11 - a21*b21 + a31*b31;

  if (det==0)
    return;

  _a = ( b11*xy[0] - b21*xy[1] + b31*xy[2])/det;
  _b = (-b21*xy[0] + b22*xy[1] - b32*xy[2])/det;
  _c = ( b31*xy[0] - b32*xy[1] + b33*xy[2])/det;

  if (verbose) {
    printf("%g\t%g\t%g\n",
           ( a11*b11 - a21*b21 + a31*b31)/det,
           (-a11*b21 + a21*b22 - a31*b32)/det,
           ( a11*b31 - a21*b32 + a31*b33)/det);
    printf("%g\t%g\t%g\n",
           ( a21*b11 - a22*b21 + a32*b31)/det,
           (-a21*b21 + a22*b22 - a32*b32)/det,
           ( a21*b31 - a22*b32 + a32*b33)/det);
    printf("%g\t%g\t%g\n",
           ( a31*b11 - a32*b21 + a33*b31)/det,
           (-a31*b21 + a32*b22 - a33*b32)/det,
           ( a31*b31 - a32*b32 + a33*b33)/det);
  }
      
  if (_c<0)
    _valid=true;
}
