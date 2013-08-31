#include "SimApp.hh"

#include "pdsdata/xtc/TypeId.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/Dgram.hh"
#include "pdsdata/psddl/camera.ddl.h"
#include "pdsdata/xtc/XtcIterator.hh"

namespace Pds {
  class MyIter : public XtcIterator {
  public:
    MyIter() {}
  public:
    int process(Xtc* xtc)
    {
      if (xtc->contains.id()==TypeId::Id_Xtc)
	iterate(xtc);
      else if (xtc->contains.id()==TypeId::Id_Frame) {
	Camera::FrameV1& f = *reinterpret_cast<Camera::FrameV1*>(xtc->payload());
	uint16_t* d = const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(f.data16().data()));
	{ uint16_t* p = d + 515*f.width();
	  for(unsigned i=0; i<f.width(); i++)
	    *p++ = 256;
	}
	{ static unsigned _v(0);
          _v = (_v + 1) % f.width();
	  uint16_t* p = d + 555*f.width();
	  unsigned i=0;
	  do {
	    *p++ = 128;
	  }  while(++i<_v);
	  do {
	    *p++ = 256;
	  } while (++i<f.width());
	}
      }
      return 0;
    }
  };
};

using namespace Pds;

SimApp::SimApp()
{
}

SimApp::~SimApp()
{
}

Transition* SimApp::transitions(Transition* tr)
{
  return tr;
}

InDatagram* SimApp::events(InDatagram* dg)
{
  if (dg->datagram().seq.service()==TransitionId::L1Accept) {
    MyIter it;
    it.iterate(&dg->datagram().xtc);
  }
  return dg;
}

Occurrence* SimApp::occurrences(Occurrence* occ) {
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new SimApp; }

extern "C" void destroy(Appliance* p) { delete p; }
