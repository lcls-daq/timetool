#ifndef SimApp_hh
#define SimApp_hh

#include "pds/utility/Appliance.hh"
#include "pds/service/Semaphore.hh"

namespace Pds {
  class SimApp : public Appliance {
  public:
    SimApp();
    ~SimApp();
  public:
    Transition* transitions(Transition*);
    InDatagram* events     (InDatagram*);
    Occurrence* occurrences(Occurrence*);
  private:
  };
};

#endif
