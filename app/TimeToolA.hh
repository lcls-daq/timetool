#ifndef TimeToolA_hh
#define TimeToolA_hh

#include "pds/utility/Appliance.hh"
#include "pds/service/Semaphore.hh"

namespace Pds {
  class FexApp;
  class Task;
  class TimeToolA : public Appliance {
  public:
    TimeToolA();
    ~TimeToolA();
  public:
    Transition* transitions(Transition*);
    InDatagram* events     (InDatagram*);
    Occurrence* occurrences(Occurrence*);
  private:
    Task*     _task;
    FexApp*   _fex;
    Semaphore _sem;
    uint32_t  _bykik;
    uint32_t  _no_laser;
  };
};

#endif
