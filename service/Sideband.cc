#include "Sideband.hh"

using namespace TimeTool;

Sideband::Sideband(unsigned cols) :
  _f1  (0.05),
  _init(false),
  _cols(cols),
  _wf  (new double[cols]),
  _avg (new double[cols]) 
{
}

Sideband::~Sideband() 
{
  delete[] _wf;
  delete[] _avg;
}

double* Sideband::process(const uint32_t* wf)
{
  int cols = int(_cols);
  if (!_init) {
    _init=true;
    for(int k=0; k<cols; k++)
      _avg[k] = double(wf[k]);
  }
  else {
    const double f0 = 1-_f1;
    const double f1 = _f1;
    for(int k=0; k<cols; k++)
      _avg[k] = f0*_avg[k] + f1*double(wf[k]);
  }
  //
  // Calculate left,right x odd,even pixel baseline shifts
  //
  q_left_odd=0; q_left_even=0; q_right_odd=0; q_right_even=0;
  {
    int k=0, m=cols/2;
    while(k < cols/2) {
      q_left_even  += double(wf[k]) - _avg[k]; k++;
      q_right_even += double(wf[m]) - _avg[m]; m++;
      q_left_odd   += double(wf[k]) - _avg[k]; k++;
      q_right_odd  += double(wf[m]) - _avg[m]; m++;
    }
      
    { double n = double(cols/4);
      q_left_even  /= n;
      q_left_odd   /= n;
      q_right_even /= n;
      q_right_odd  /= n; }
  }
  {
    int k=0, m=cols/2;
    while(k < cols/2) {
      _wf[k] = _avg[k] + q_left_even; k++;
      _wf[m] = _avg[m] + q_right_even; m++;
      _wf[k] = _avg[k] + q_left_odd; k++;
      _wf[m] = _avg[m] + q_right_odd; m++;
    }
  }
  return _wf;
}
