#ifndef TimeToolB_hh
#define TimeToolB_hh

#include "pds/client/WorkThreads.hh"

#include "pdsdata/psddl/evr.ddl.h"
#include "ndarray/ndarray.h"

namespace Pds {
  class FexApp;
  class TimeToolB : public WorkThreads {
  public:
    TimeToolB();
    ~TimeToolB();
  public:
    InDatagram* events     (InDatagram*);
    Occurrence* occurrences(Occurrence*);
  private:
    FexApp*   _fex;
    std::vector< std::vector<Pds::EvrData::FIFOEvent> > _evr;
  };
};

#endif
