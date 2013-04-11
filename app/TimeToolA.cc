#include "TimeToolA.hh"

#include "pds/service/Task.hh"
#include "pds/service/TaskObject.hh"
#include "pds/service/Routine.hh"
#include "pds/utility/NullServer.hh"
#include "pds/utility/Occurrence.hh"
#include "pds/utility/OccurrenceId.hh"
#include "pds/epicstools/PVWriter.hh"

#include "pdsdata/xtc/TypeId.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/Dgram.hh"
#include "pdsdata/xtc/XtcIterator.hh"
#include "pdsdata/xtc/TransitionId.hh"
#include "pdsdata/camera/FrameV1.hh"
#include "pdsdata/opal1k/ConfigV1.hh"
#include "pdsdata/evr/DataV3.hh"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/ProcInfo.hh"

#include "pdsdata/encoder/ConfigV2.hh"
#include "pdsdata/encoder/DataV2.hh"
#include "alarm.h"
#include "pdsdata/epics/EpicsDbrTools.hh"
#include "pdsdata/epics/EpicsPvData.hh"

#include "timetool/service/Fex.hh"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <new>

using std::string;

typedef Pds::Opal1k::ConfigV1 Opal1kConfig;
typedef Pds::EvrData::DataV3 EvrDataType;

using namespace Pds;
using Pds_Epics::PVWriter;

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
  
  class FexApp : public XtcIterator {
  public:
    FexApp(const char* fname) : _fex(fname), _pv_writer(0) {}
    ~FexApp() {}
  public:
    Transition* transitions(Transition* tr) {
      if (tr->id()==TransitionId::Configure) {
        _fex.configure();
        _adjust_n = 0;
        _adjust_v = 0;
        if (_fex._adjust_stats) {
          _pv_writer = new PVWriter(_fex._adjust_pv.c_str());
        }
      }
      else if (tr->id()==TransitionId::Unconfigure) {
        _fex.unconfigure();
        if (_pv_writer) {
          delete _pv_writer;
          _pv_writer = 0;
        }
      }
      return tr;
    }
    InDatagram* events(InDatagram* dg) {
      switch(dg->datagram().seq.service()) {
      case TransitionId::L1Accept:
        { //  Add an encoder data object
          _fex.reset();

          const Src& src = reinterpret_cast<Xtc*>(dg->xtc.payload())->src;

          _frame = 0;
          iterate(&dg->xtc);

          if (_frame) {
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

          if (_fex.status()) {
            _adjust_v += _fex.filtered_pos_adj();
            _adjust_n++;
            if (_adjust_n==_fex._adjust_stats) {
              *reinterpret_cast<double*>(_pv_writer->data()) = 1.e3*_adjust_v/double(_adjust_n);
              _pv_writer->put();
              _adjust_v = 0;
              _adjust_n = 0;
            }
          }

          break; }
      case TransitionId::Configure:
        { //  Add an encoder config object
          const ProcInfo& info = reinterpret_cast<const ProcInfo&>(dg->xtc.src);
          DetInfo src  (info.processId(),
                       (DetInfo::Detector)((_fex._phy>>24)&0xff), ((_fex._phy>>16)&0xff),
                       (DetInfo::Device  )((_fex._phy>> 8)&0xff), ((_fex._phy>> 0)&0xff));
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
    int process(Xtc* xtc) {
      if (xtc->contains.id()==TypeId::Id_Xtc) 
        iterate(xtc);
      else if (xtc->contains.id()==TypeId::Id_Frame) {
        _frame = reinterpret_cast<const Camera::FrameV1*>(xtc->payload());
      }
      return 1;
    }
    unsigned event_code_bykik   () const { return _fex._event_code_bykik; }
    unsigned event_code_no_laser() const { return _fex._event_code_no_laser; }
    void setup(bool bykik, bool no_laser) { _bykik=bykik; _no_laser=no_laser; }
  private:
    TimeTool::Fex _fex;
    const Camera::FrameV1* _frame;
    bool      _bykik;
    bool      _no_laser;
    PVWriter* _pv_writer;
    unsigned  _adjust_n;
    double    _adjust_v;
  };


  class QueuedFex : public Routine {
  public:
    QueuedFex(InDatagram* dg, TimeToolA* app, FexApp* fex, 
              bool bykik, bool no_laser)
      : _dg(dg), _app(app), _fex(fex), 
        _bykik(bykik), _no_laser(no_laser), _sem(0) {}
    QueuedFex(InDatagram* dg, TimeToolA* app, FexApp* fex, Semaphore* sem) 
      : _dg(dg), _app(app), _fex(fex), 
        _bykik(false), _no_laser(false), _sem(sem) {}
    ~QueuedFex() {}
  public:
    void routine() {

      _fex->setup(_bykik,_no_laser);
      _app->post(_fex->events(_dg));

      if (_sem) _sem->give();
      delete this;
    }
  private:
    InDatagram* _dg;
    TimeToolA*   _app;
    FexApp*     _fex;
    bool        _bykik;
    bool        _no_laser;
    Semaphore*  _sem;
  };
};

TimeToolA::TimeToolA() :
  _task(new Task(TaskObject("ttool"))),
  _fex (new FexApp("timetool.input")),
  _sem (Semaphore::EMPTY),
  _bykik(0),
  _no_laser(0)
{
}

TimeToolA::~TimeToolA()
{
  delete _fex;
}

Transition* TimeToolA::transitions(Transition* tr)
{
  return _fex->transitions(tr);
}

InDatagram* TimeToolA::events(InDatagram* dg)
{
  if (dg->datagram().seq.service()==TransitionId::L1Accept) {
    uint32_t b = 1 << (dg->datagram().seq.stamp().vector()&0x1f);
    bool bykik    = _bykik & b;
    bool no_laser = _no_laser & b;
    _task->call(new QueuedFex(dg, this, _fex, bykik, no_laser));
    _bykik    &= ~b;
    _no_laser &= ~b;
  }
  else {
    _task->call(new QueuedFex(dg, this, _fex, &_sem));
    _sem.take();
  }
  return (InDatagram*)Appliance::DontDelete;
}

Occurrence* TimeToolA::occurrences(Occurrence* occ) {
  if (occ->id() == OccurrenceId::EvrCommand) {
    const EvrCommand& cmd = *reinterpret_cast<const EvrCommand*>(occ);
    if (cmd.code == _fex->event_code_bykik())
      _bykik    |= 1<<(cmd.seq.stamp().vector()&0x1f);
    if (cmd.code == _fex->event_code_no_laser())
      _no_laser |= 1<<(cmd.seq.stamp().vector()&0x1f);
  }
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new TimeToolA; }

extern "C" void destroy(Appliance* p) { delete p; }
