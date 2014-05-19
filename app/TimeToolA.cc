#include "TimeToolA.hh"

#include "pds/service/Task.hh"
#include "pds/service/TaskObject.hh"
#include "pds/service/Routine.hh"
#include "pds/utility/NullServer.hh"
#include "pds/utility/Occurrence.hh"
#include "pds/utility/OccurrenceId.hh"

#include "pdsdata/xtc/TypeId.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/Dgram.hh"
#include "pdsdata/xtc/XtcIterator.hh"
#include "pdsdata/xtc/TransitionId.hh"
#include "pdsdata/compress/CompressedXtc.hh"
#include "pdsdata/psddl/camera.ddl.h"
#include "pdsdata/psddl/opal1k.ddl.h"
#include "pdsdata/psddl/evr.ddl.h"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/ProcInfo.hh"

#include "pdsdata/psddl/encoder.ddl.h"
#include "pdsdata/psddl/epics.ddl.h"

#include "pds/epicstools/PVWriter.hh"
#include "timetool/service/Fex.hh"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <new>
#include <fstream>

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
  unsigned sz = sizeof(Pds::Epics::EpicsPvCtrlDouble)+sizeof(double);
  sz = (sz+3)&~3;

  double data(0);
  char* p = new char[sz];
  
  Pds::Epics::dbr_ctrl_double ctrl; memset(&ctrl, 0, sizeof(ctrl));
  new(p) Pds::Epics::EpicsPvCtrlDouble(id,DBR_CTRL_DOUBLE,1,name.c_str(),ctrl,&data);
  
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

static void _insert_projection(InDatagram* dg,
			       const Src&  src,
			       int         id,
			       const string& name)
{
  const int wf_size=1024;
  unsigned sz = sizeof(Pds::Epics::EpicsPvCtrlLong)+wf_size*sizeof(unsigned);
  sz = (sz+3)&~3;
  char* p = new char[sz];

  Pds::Epics::dbr_ctrl_long ctrl; memset(&ctrl, 0, sizeof(ctrl));
  int data[wf_size];
  memset(data,0,wf_size*sizeof(int));

  new(p) Pds::Epics::EpicsPvCtrlLong(id,DBR_CTRL_LONG,wf_size,name.c_str(),ctrl,data);
  
  Xtc xtc(TypeId(TypeId::Id_Epics,1),src);
  xtc.extent += sz;
  dg->insert(xtc, p);
  delete[] p;
}

static void _insert_projection(InDatagram* dg,
			       const Src&  src,
			       int         id,
			       const uint32_t* val)
{
#define PvType Pds::Epics::EpicsPvTimeLong

  const int wf_size=1024;

  Xtc& xtc = dg->xtc;
  unsigned extent = xtc.extent;

  Xtc* tc = new (&xtc) Xtc(TypeId(TypeId::Id_Epics,1),src);
  unsigned payload_size = sizeof(PvType)+sizeof(unsigned)*wf_size;
  payload_size = (payload_size+3)&~3;
  char* b = (char*)xtc.alloc(payload_size);

  Pds::Epics::dbr_time_long v;
  memset(&v, 0, sizeof(v));
  new (b) PvType(id, DBR_TIME_LONG, wf_size, v, reinterpret_cast<const int32_t*>(val));

  tc->extent = xtc.extent - extent;

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
    FexApp(const char* fname) : _pv_writer(0) {
      char buff[128];
      const char* dir = getenv("HOME");
      sprintf(buff,"%s/%s", dir ? dir : "/tmp", fname);
      std::ifstream f(buff);
      if (f) {
        while(!f.eof()) {
          string sline;
          std::getline(f,sline);
          if (sline[0]=='<')
            _fex.push_back(new TimeTool::Fex(sline.substr(1).c_str()));
        }
        if (_fex.size()==0)
          _fex.push_back(new TimeTool::Fex(fname));
        _frame    .resize(_fex.size());
        _pv_writer.resize(_fex.size());
        _pXtc     .resize(_fex.size());
      }
    }
    ~FexApp() {
      for(unsigned i=0; i<_fex.size(); i++) {
        delete _fex[i];
        if (_pv_writer[i])
          delete _pv_writer[i];
      }
    }
  public:
    Transition* transitions(Transition* tr) {
      if (tr->id()==TransitionId::Configure) {
        for(unsigned i=0; i<_fex.size(); i++) {
          // cleanup
          if (_pv_writer[i]) {
            delete _pv_writer[i];
            _pv_writer[i] = 0;
          }
          // configure
          TimeTool::Fex& fex = *_fex[i];
          fex.configure();
//           if (fex._adjust_stats)
//             _pv_writer[i] = new PVWriter(fex._adjust_pv.c_str());
        }
        _adjust_n = 0;
        _adjust_v = 0;
      }
      else if (tr->id()==TransitionId::Unconfigure) {
        for(unsigned i=0; i<_fex.size(); i++) {
          _fex[i]->unconfigure();
          if (_pv_writer[i]) {
            delete _pv_writer[i];
            _pv_writer[i] = 0;
          }
        }
      }
      return tr;
    }
    InDatagram* events(InDatagram* dg,
                       const ndarray<const Pds::EvrData::FIFOEvent,1>& fifo) {
      switch(dg->datagram().seq.service()) {
      case TransitionId::L1Accept:
        { //  Add an encoder data object
          for(unsigned i=0; i<_fex.size(); i++)
            _frame[i] = 0;
          
          const Src& src = reinterpret_cast<Xtc*>(dg->xtc.payload())->src;
          
          iterate(&dg->xtc);
          
          for(unsigned i=0; i<_fex.size(); i++) {
            if (_frame[i]) {
              TimeTool::Fex& fex = *_fex[i];
              fex.reset();
              
              fex.analyze(_frame[i]->data16(), fifo, 0);
                          
              // assumes only one fex per event
              if (!fex.write_image()) {
                uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
                FrameTrim iter(pdg);
                iter.process(&dg->xtc);
              }

              /*
              if (fex.write_projections()) {
                _insert_projection(dg, src, 6, fex.signal_wf   ());
                _insert_projection(dg, src, 7, fex.sideband_wf ());
                _insert_projection(dg, src, 8, fex.reference_wf());
              }
              */
              Damage dmg(fex.status() ? 0x4000 : 0);

              //  Remove the frame
              //	  dg->datagram().xtc.extent = sizeof(Xtc);

              //  Insert the results
              _insert_pv(dg, src, 0, fex.amplitude());
              _insert_pv(dg, src, 1, fex.filtered_position ());
              _insert_pv(dg, src, 2, fex.filtered_pos_ps ());
              _insert_pv(dg, src, 3, fex.filtered_fwhm ());
              _insert_pv(dg, src, 4, fex.next_amplitude());
              _insert_pv(dg, src, 5, fex.ref_amplitude());

              if (fex.status()) {
//                 _adjust_v += fex.filtered_pos_adj();
//                 _adjust_n++;
//                 if (_adjust_n==fex._adjust_stats) {
//                   *reinterpret_cast<double*>(_pv_writer[i]->data()) = 1.e3*_adjust_v/double(_adjust_n);
//                   _pv_writer[i]->put();
//                   _adjust_v = 0;
//                   _adjust_n = 0;
//                 }
              }
              break; 
            }
          }

          break; }
      case TransitionId::Configure:
        _dg = dg;
        iterate(&dg->xtc);
        break;
      default:
        break;
      }

      return dg;
    }
    int process(Xtc* xtc) {
      if (xtc->contains.id()==TypeId::Id_Xtc) 
        iterate(xtc);
      else if (xtc->contains.id()==TypeId::Id_Frame) {
        for(unsigned i=0; i<_fex.size(); i++)
          if (_fex[i]->m_get_key == xtc->src.phy()) {
            if (xtc->contains.compressed()) {
              _pXtc [i] = Pds::CompressedXtc::uncompress(*xtc);
              _frame[i] = reinterpret_cast<Pds::Camera::FrameV1*>(_pXtc[i]->payload());
            }
            else
              _frame[i] = reinterpret_cast<const Camera::FrameV1*>(xtc->payload());
          }
      }
      else if (xtc->contains.id()==TypeId::Id_Opal1kConfig) {
        for(unsigned i=0; i<_fex.size(); i++) {
          TimeTool::Fex& fex = *_fex[i];
          if (fex.m_get_key == xtc->src.phy()) {
            InDatagram* dg = _dg;
            const Src& src = fex.src();
            _insert_pv(dg, src, 0, fex.base_name()+":AMPL");
            _insert_pv(dg, src, 1, fex.base_name()+":FLTPOS");
            _insert_pv(dg, src, 2, fex.base_name()+":FLTPOS_PS");
            _insert_pv(dg, src, 3, fex.base_name()+":FLTPOSFWHM");
            _insert_pv(dg, src, 4, fex.base_name()+":AMPLNXT");
            _insert_pv(dg, src, 5, fex.base_name()+":REFAMPL");

            if (fex.write_projections()) {
              _insert_projection(dg, src, 6, fex.base_name()+":SIGNAL_WF");
              _insert_projection(dg, src, 7, fex.base_name()+":SIDEBAND_WF");
              _insert_projection(dg, src, 8, fex.base_name()+":REFERENCE_WF");
            }
          }
        }
      }
      return 1;
    }
  private:
    std::vector<TimeTool::Fex*        > _fex;
    std::vector<const Camera::FrameV1*> _frame;
    std::vector<PVWriter*             > _pv_writer;
    std::vector<boost::shared_ptr<Pds::Xtc> > _pXtc;
    unsigned  _adjust_n;
    double    _adjust_v;
    InDatagram* _dg;
  };


  class QueuedFex : public Routine {
  public:
    QueuedFex(InDatagram* dg, TimeToolA* app, FexApp* fex, 
              std::vector<Pds::EvrData::FIFOEvent>& fifo)
      : _dg(dg), _app(app), _fex(fex), 
        _fifo(make_ndarray<Pds::EvrData::FIFOEvent>(fifo.size())),
        _sem(0) 
    {
      for(unsigned i=0; i<fifo.size(); i++)
        _fifo[i] = fifo[i];
    }
    QueuedFex(InDatagram* dg, TimeToolA* app, FexApp* fex, Semaphore* sem) 
      : _dg(dg), _app(app), _fex(fex), 
        _sem(sem) {}
    ~QueuedFex() {}
  public:
    void routine() {

      _app->post(_fex->events(_dg,_fifo));

      if (_sem) _sem->give();
      delete this;
    }
  private:
    InDatagram* _dg;
    TimeToolA*   _app;
    FexApp*     _fex;
    ndarray<Pds::EvrData::FIFOEvent,1> _fifo;
    Semaphore*  _sem;
  };
};

TimeToolA::TimeToolA() :
  _task(new Task(TaskObject("ttool"))),
  _fex (new FexApp("timetool.input")),
  _sem (Semaphore::EMPTY),
  _evr (32)
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
    uint32_t b = (dg->datagram().seq.stamp().vector()&0x1f);
    _task->call(new QueuedFex(dg, this, _fex, _evr[b]));
    _evr[b].clear();
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
    _evr[(cmd.seq.stamp().vector()&0x1f)].push_back(Pds::EvrData::FIFOEvent(0,0,cmd.code));
  }
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new TimeToolA; }

extern "C" void destroy(Appliance* p) { delete p; }
