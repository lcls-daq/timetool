#ifndef TimeToolA_hh
#define TimeToolA_hh

#include "pds/utility/Appliance.hh"
#include "pds/service/Semaphore.hh"
#include "pdsdata/psddl/evr.ddl.h"
#include "ndarray/ndarray.h"

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
    std::vector< std::vector<Pds::EvrData::FIFOEvent> > _evr;
  };
};

#endif
