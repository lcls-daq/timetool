#ifndef TimeTool_Sideband_hh
#define TimeTool_Sideband_hh

#include <stdint.h>

namespace TimeTool {
  class Sideband {
  public:
    Sideband(unsigned);
    ~Sideband();
  public:
    double* process(const uint32_t* wf);
    void set_convergence(double v) { _f1=v; }
  private:
    double _f1;
    bool _init;
    unsigned _cols;
    double q_left_even;
    double q_left_odd;
    double q_right_even;
    double q_right_odd;
    double* _wf;
    double* _avg;
  };
};

#endif
