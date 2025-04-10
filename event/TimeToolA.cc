#include "TimeToolA.hh"

#include "pdsdata/xtc/TypeId.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/Dgram.hh"
#include "pdsdata/xtc/TransitionId.hh"
#include "pdsdata/compress/CompressedXtc.hh"
#include "pdsdata/psddl/opal1k.ddl.h"
#include "pdsdata/psddl/evr.ddl.h"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/ProcInfo.hh"

#include "pdsdata/psddl/lusi.ddl.h"
#include "pdsdata/psddl/epics.ddl.h"

#include "pds/epicstools/PVWriter.hh"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <new>

using std::string;

typedef Pds::EvrData::DataV3 EvrDataType;

using namespace Pds;

//#define DBUG

static void _insert_pv(InDatagram* dg,
                       const Src&  src,
                       int         id,
                       const string& name)
{
  unsigned sz = sizeof(Pds::Epics::EpicsPvCtrlDouble)+sizeof(double);
  sz = (sz+3)&~3;

  double data(0);
  char* p = new char[sz];

  char cname[Pds::Epics::iMaxPvNameLength];
  memset(cname,0,Pds::Epics::iMaxPvNameLength);
  sprintf(cname,"%s",name.c_str());

  Pds::Epics::dbr_ctrl_double ctrl; memset(&ctrl, 0, sizeof(ctrl));
  new(p) Pds::Epics::EpicsPvCtrlDouble(id,DBR_CTRL_DOUBLE,1,cname,ctrl,&data);
  
  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += sz;
  dg->insert(xtc, p);
  delete[] p;
}

static void _insert_pv(InDatagram* dg,
                       const Src&  src,
                       int         id,
                       double      val)
{
  Pds::Epics::dbr_time_double v;
  memset(&v,0,sizeof(v));
  
  unsigned sz = sizeof(Pds::Epics::EpicsPvTimeDouble)+sizeof(double);
  sz = (sz+3)&~3;

  char* p = new char[sz];
  new(p) Pds::Epics::EpicsPvTimeDouble(id,DBR_TIME_DOUBLE,1,v,&val);

  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += sz;
  dg->insert(xtc, p);
  delete[] p;
}

static ndarray<const EvrData::FIFOEvent, 1> _trimfifoEvents(uint32_t fid, Pds::EvrData::DataV3* evr)
{
  unsigned ntrim = 0;
  ndarray<const Pds::EvrData::FIFOEvent, 1> fifo = evr->fifoEvents();
  for (unsigned n=0; n<evr->numFifoEvents(); n++) {
    if (fifo[n].timestampHigh() == fid) {
      ntrim++;
    }
  }

  ndarray<Pds::EvrData::FIFOEvent, 1> trim = make_ndarray<Pds::EvrData::FIFOEvent>(ntrim);
  for (unsigned n=0,m=0; n<evr->numFifoEvents(); n++) {
    if (fifo[n].timestampHigh() == fid) {
      trim[m++] = fifo[n];
    }
  }

  return trim;
}

namespace Pds {

  class FrameTrim {
  public:
    FrameTrim(uint32_t*& pwrite) : _pwrite(pwrite) {}
  private:
    void _write(const void* p, ssize_t sz) {
      const uint32_t* pread = (uint32_t*)p;
      if (_pwrite!=pread) {
	const uint32_t* const end = pread+(sz>>2);
	while(pread < end)
	  *_pwrite++ = *pread++;
      }
      else
	_pwrite += sz>>2;
    }
  public:
    void iterate(Xtc* root) {
      if (root->damage.value() & ( 1 << Damage::IncompleteContribution))
	return _write(root,root->extent);

      int remaining = root->sizeofPayload();
      Xtc* xtc     = (Xtc*)root->payload();

      uint32_t* pwrite = _pwrite;
      _write(root, sizeof(Xtc));
    
      while(remaining > 0) {
	unsigned extent = xtc->extent;
	process(xtc);
	remaining -= extent;
	xtc        = (Xtc*)((char*)xtc+extent);
      }

      reinterpret_cast<Xtc*>(pwrite)->extent = (_pwrite-pwrite)*sizeof(uint32_t);
    }

  public:
    void process(Xtc* xtc) {
      switch(xtc->contains.id()) {
      case (TypeId::Id_Xtc):
	{ FrameTrim iter(_pwrite);
	  iter.iterate(xtc);
	  break; }
      case (TypeId::Id_Frame):
	break;
      default :
	_write(xtc,xtc->extent);
	break;
      }
    }
  private:
    uint32_t*& _pwrite;
  };
};


Pds_TimeTool_event::TimeToolA::TimeToolA() :
  _fex("timetool.input"),
  _frame(NULL)
{}

Pds_TimeTool_event::TimeToolA::~TimeToolA()
{
  if (_frame) delete _frame;
}

Transition* Pds_TimeTool_event::TimeToolA::transitions(Transition* tr) 
{
  if (tr->id()==TransitionId::Configure) {
    _fex.configure();
  }
  else if (tr->id()==TransitionId::Unconfigure) {
    _fex.unconfigure();
  }
  return tr;
}

InDatagram* Pds_TimeTool_event::TimeToolA::events(InDatagram* dg) 
{
  const Src& src = reinterpret_cast<Xtc*>(dg->xtc.payload())->src;

  switch(dg->datagram().seq.service()) {
  case TransitionId::L1Accept:
    {
      _fex.reset();
      
      if (_frame)
        _frame->clear_frame();
      _evrdata = 0;
      _ipmdata = 0;

      iterate(&dg->xtc);

      if (_frame && !_frame->empty() && _evrdata) {

        _fex.analyze(_frame->data(),
                     _trimfifoEvents(dg->datagram().seq.stamp().fiducials(),_evrdata),
                     _ipmdata);

        if (!_fex.write_image()) {
          uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
	        FrameTrim iter(pdg);
	        iter.process(&dg->xtc);
	      }
      }

      Damage dmg(_fex.status() ? 0x4000 : 0);
      
      //  Remove the frame
      //	  dg->datagram().xtc.extent = sizeof(Xtc);
      
      //  Insert the results
      _insert_pv(dg, src, 0, _fex.amplitude());
      _insert_pv(dg, src, 1, _fex.filtered_position ());
      _insert_pv(dg, src, 2, _fex.filtered_pos_ps ());
      _insert_pv(dg, src, 3, _fex.filtered_fwhm ());
      _insert_pv(dg, src, 4, _fex.next_amplitude());
      _insert_pv(dg, src, 5, _fex.ref_amplitude());
      _insert_pv(dg, src, 6, _fex.sig_roi_sum());

      break; }
  case TransitionId::Configure:
    {
      _insert_pv(dg, src, 0, _fex.base_name()+":AMPL");
      _insert_pv(dg, src, 1, _fex.base_name()+":FLTPOS");
      _insert_pv(dg, src, 2, _fex.base_name()+":FLTPOS_PS");
      _insert_pv(dg, src, 3, _fex.base_name()+":FLTPOSFWHM");
      _insert_pv(dg, src, 4, _fex.base_name()+":AMPLNXT");
      _insert_pv(dg, src, 5, _fex.base_name()+":REFAMPL");
      _insert_pv(dg, src, 6, _fex.base_name()+":SIGROISUM");
      break; }
  default:
    break;
  }
  
  return dg;
}

int Pds_TimeTool_event::TimeToolA::process(Xtc* xtc) 
{
  if (xtc->contains.id()==TypeId::Id_Xtc) 
    iterate(xtc);
  else if (xtc->contains.id()==TypeId::Id_VimbaFrame &&
     xtc->src.phy() == _fex.src().phy()) {
    _frame->set_frame(xtc->contains, xtc->payload());
  }
  else if (xtc->contains.id()==TypeId::Id_Frame && 
	   xtc->src.phy() == _fex.src().phy()) {
    if (xtc->contains.compressed()) {
      _pXtc = Pds::CompressedXtc::uncompress(*xtc);
      _frame->set_frame(xtc->contains, _pXtc->payload());
    }
    else
      _frame->set_frame(xtc->contains, xtc->payload());
  }
  else if (xtc->contains.id()==Pds::TypeId::Id_EvrData) {
    _evrdata = reinterpret_cast<Pds::EvrData::DataV3*>(xtc->payload());
  }
  else if (xtc->contains.id()==Pds::TypeId::Id_AlviumConfig &&
      xtc->src.phy() == _fex.src().phy()) {
    _frame = ::TimeTool::FrameCache::instance(xtc->src, xtc->contains, xtc->payload());
  }
  else if (xtc->contains.id()==Pds::TypeId::Id_Opal1kConfig &&
      xtc->src.phy() == _fex.src().phy()) {
    _frame = ::TimeTool::FrameCache::instance(xtc->src, xtc->contains, xtc->payload());
  }
  else if (xtc->src.level()==_fex.m_ipm_get_key.level() && 
	   xtc->src.phy  ()==_fex.m_ipm_get_key.phy  () &&
	   xtc->contains.id  ()==Pds::TypeId::Id_IpmFex) {
    _ipmdata = reinterpret_cast<Pds::Lusi::IpmFexV1*>(xtc->payload());
  }
  return 1;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new Pds_TimeTool_event::TimeToolA; }

extern "C" void destroy(Appliance* p) { delete p; }
