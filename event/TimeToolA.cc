#include "TimeToolA.hh"

#include "pdsdata/xtc/TypeId.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/Dgram.hh"
#include "pdsdata/xtc/TransitionId.hh"
#include "pdsdata/opal1k/ConfigV1.hh"
#include "pdsdata/evr/DataV3.hh"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/ProcInfo.hh"

#include "pdsdata/camera/FrameV1.hh"
#include "pdsdata/opal1k/ConfigV1.hh"
#include "pdsdata/evr/DataV3.hh"
#include "pdsdata/lusi/IpmFexV1.hh"

#include "pds/epicstools/PVWriter.hh"
#include "alarm.h"
#include "pdsdata/epics/EpicsDbrTools.hh"
#include "pdsdata/epics/EpicsPvData.hh"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <new>

using std::string;

typedef Pds::Opal1k::ConfigV1 Opal1kConfig;
typedef Pds::EvrData::DataV3 EvrDataType;

using namespace Pds;

static void _insert_pv(InDatagram* dg,
                       const Src&  src,
                       int         id,
                       const string& name)
{
  Pds::EpicsPvCtrlHeader payload(id,DBR_CTRL_DOUBLE,1,name.c_str());
  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += (sizeof(payload)+3)&(~3);
  dg->insert(xtc, &payload);
}

static void _insert_pv(InDatagram* dg,
                       const Src&  src,
                       int         id,
                       double      val)
{
  //  Pds::Epics::dbr_time_double v;
  dbr_time_double v;
  memset(&v,0,sizeof(v));
  v.value = val;
  Pds::EpicsPvTime<DBR_DOUBLE> payload(id,1,&v);
  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += (sizeof(payload)+3)&(~3);
  dg->insert(xtc, &payload);
}

static void _insert_projection(InDatagram* dg,
			       const Src&  src,
			       int         id,
			       const string& name)
{
  const int wf_size=1024;
  Pds::EpicsPvCtrlHeader payload(id,DBR_CTRL_LONG,wf_size,name.c_str());
  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += (sizeof(payload)+3)&(~3);
  dg->insert(xtc, &payload);
}

static void _insert_projection(InDatagram* dg,
			       const Src&  src,
			       int         id,
			       const uint32_t* val)
{
#define PvType Pds::EpicsPvTime<DBR_LONG>
#define BaseType EpicsDbrTools::DbrTypeTraits<DBR_LONG>::TDbrTime

  const int wf_size=1024;

  Xtc& xtc = dg->xtc;
  unsigned extent = xtc.extent;


  Xtc* tc = new (&xtc) Xtc(TypeId(TypeId::Id_Epics,1),src);
  unsigned payload_size = sizeof(PvType)+sizeof(uint32_t)*(wf_size-1);
  payload_size = (payload_size+3)&~3;
  char* b = (char*)xtc.alloc(payload_size);

  PvType* p;
  { dbr_time_long v;
    memset(&v,0,sizeof(v));
    v.value = val[0];
    p = new (b) PvType( id, 1, &v);
    b += sizeof(PvType); }

  memcpy( p+1, &val[1], sizeof(uint32_t)*(wf_size-1) );

  tc->extent = xtc.extent - extent;

#undef ValueType
#undef BaseType
#undef PvType
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


Pds_TimeTool_event::TimeToolA::TimeToolA() : _fex("timetool.input") {}

Pds_TimeTool_event::TimeToolA::~TimeToolA() {}

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
      
      _frame = 0;
      _evrdata = 0;
      _ipmdata = 0;

      iterate(&dg->xtc);

      if (_frame && _evrdata) {

	bool bykik    = false;
	bool no_laser = false;

	unsigned laser_code = abs(_fex._event_code_no_laser);
	for(unsigned i=0; i<_evrdata->numFifoEvents(); i++) {
	  const EvrDataType::FIFOEvent& fe = _evrdata->fifoEvent(i);
	  if (fe.EventCode == _fex._event_code_bykik)
	    bykik = true;
	  if (fe.EventCode == laser_code)
	    no_laser = true;
	}
	
	if (int(_fex._event_code_no_laser) < 0)
	  no_laser = !no_laser;
	
	if (_ipmdata) {
	  if (_ipmdata->sum < _fex._ipm_no_beam_threshold)
	    bykik = true;
	}
	
	_fex.analyze(*_frame,_bykik,_no_laser);
	
	if (!_fex.write_image()) {
	  uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
	  FrameTrim iter(pdg);
	  iter.process(&dg->xtc);
	}

	if (_fex.write_projections()) {
	  _insert_projection(dg, src, 6, _fex.signal_wf   ());
	  _insert_projection(dg, src, 7, _fex.sideband_wf ());
	  _insert_projection(dg, src, 8, _fex.reference_wf());
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

      break; }
  case TransitionId::Configure:
    {
      _insert_pv(dg, src, 0, _fex.base_name()+":AMPL");
      _insert_pv(dg, src, 1, _fex.base_name()+":FLTPOS");
      _insert_pv(dg, src, 2, _fex.base_name()+":FLTPOS_PS");
      _insert_pv(dg, src, 3, _fex.base_name()+":FLTPOSFWHM");
      _insert_pv(dg, src, 4, _fex.base_name()+":AMPLNXT");
      _insert_pv(dg, src, 5, _fex.base_name()+":REFAMPL");
      
      if (_fex.write_projections()) {
	_insert_projection(dg, src, 6, _fex.base_name()+":SIGNAL_WF");
	_insert_projection(dg, src, 7, _fex.base_name()+":SIDEBAND_WF");
	_insert_projection(dg, src, 8, _fex.base_name()+":REFERENCE_WF");
      }
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
  else if (xtc->contains.id()==TypeId::Id_Frame && 
	   xtc->src.phy() == _fex._phy) {
    _frame = reinterpret_cast<const Camera::FrameV1*>(xtc->payload());
  }
  else if (xtc->contains.id()==Pds::TypeId::Id_EvrData) {
    _evrdata = reinterpret_cast<Pds::EvrData::DataV3*>(xtc->payload());
  }
  else if (xtc->src.level()==_fex._ipm_no_beam_src.level() && 
	   xtc->src.phy  ()==_fex._ipm_no_beam_src.phy  () &&
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
