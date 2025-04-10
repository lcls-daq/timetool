#include "TimeToolB.hh"

#include "pds/client/FrameTrim.hh"
#include "pds/service/Task.hh"
#include "pds/service/TaskObject.hh"
#include "pds/service/Routine.hh"
#include "pds/service/GenericPool.hh"
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

#include "psalg/psalg.h"

#include "pds/epicstools/PVWriter.hh"
#include "timetool/service/Fex.hh"
#include "timetool/service/FrameCache.hh"

#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <string>
#include <new>
#include <fstream>
#include <map>

#define NWORK_THREADS 8

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

namespace Pds {

  //
  //  A class to collect the references from each of the threads
  //
  class Fex    : public ::TimeTool::Fex {
  public:
    Fex(const char* fname);
    ~Fex();
  public:
    void _monitor_raw_sig (const ndarray<const double,1>&);
    void _monitor_ref_sig (const ndarray<const double,1>&);
  };
    
  //
  //  The appliance that runs in each thread
  //
  class FexApp : public Appliance, public XtcIterator {
  public:
    FexApp(const char* fname, Appliance& app) : 
      _app      (app),
      _occPool  (sizeof(UserMessage),4),
      _pv_writer(0) 
    {
      char buff[PATH_MAX];
      const char* dir = ::TimeTool::default_file_path();
      sprintf(buff,"%s/%s", dir, fname);
      std::ifstream f(buff);
      if (f) {
        while(!f.eof()) {
          string sline;
          std::getline(f,sline);
          if (sline[0]=='<')
            _fex.push_back(new Fex(sline.substr(1).c_str()));
        }
        if (_fex.size()==0)
          _fex.push_back(new Fex(fname));
        _frame    .resize(_fex.size());
        _pv_writer.resize(_fex.size());
        _pXtc     .resize(_fex.size());
      }
    }
    ~FexApp() {
      for(unsigned i=0; i<_fex.size(); i++) {
        delete _fex[i];
        if (_frame[i])
          delete _frame[i];
        if (_pv_writer[i])
          delete _pv_writer[i];
      }
    }
  public:
    Transition* transitions(Transition* tr) {
      if (tr->id()==TransitionId::Configure) {
        std::vector<unsigned> requested_codes;

#define INSERT_CODE(c) {                                                \
          if (c) {                                                      \
            bool lfound=false;                                          \
            unsigned co=abs(c);                                         \
            for(unsigned i=0; i<requested_codes.size(); i++)            \
              if (requested_codes[i]==co) { lfound=true; break; }       \
            if (!lfound) requested_codes.push_back(co); } }

        for(unsigned i=0; i<_fex.size(); i++) {
          // cleanup
          if (_pv_writer[i]) {
            delete _pv_writer[i];
            _pv_writer[i] = 0;
          }
          // configure
          ::TimeTool::Fex& fex = *_fex[i];
          fex.configure();
	  for(unsigned k=0; k<fex.m_beam_logic.size(); k++)
	    INSERT_CODE(fex.m_beam_logic[k].event_code());
	  for(unsigned k=0; k<fex.m_laser_logic.size(); k++)
	    INSERT_CODE(fex.m_laser_logic[k].event_code());
//           if (fex._adjust_stats)
//             _pv_writer[i] = new PVWriter(fex._adjust_pv.c_str());
        }

        if (requested_codes.size())
          _app.post(new(&_occPool) EvrCommandRequest(requested_codes));

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
    InDatagram* events(InDatagram* dg) {
      switch(dg->datagram().seq.service()) {
      case TransitionId::L1Accept:
        { //  Add an encoder data object
          for(unsigned i=0; i<_fex.size(); i++)
            if (_frame[i])
              _frame[i]->clear_frame();
          
          const Src& src = reinterpret_cast<Xtc*>(dg->xtc.payload())->src;
          
          iterate(&dg->xtc);

          //
          //  Decode the EVR FIFO from the tail of the datagram
          //
          const uint32_t* v = reinterpret_cast<const uint32_t*>
            (dg->xtc.payload()+dg->xtc.sizeofPayload());
          ndarray<Pds::EvrData::FIFOEvent,1> fifo = 
            make_ndarray<Pds::EvrData::FIFOEvent>(v[0]);
          for(unsigned i=0; i<v[0]; i++)
            fifo[i] = Pds::EvrData::FIFOEvent(0,0,v[i+1]);

          for(unsigned i=0; i<_fex.size(); i++) {
            if (_frame[i] && !_frame[i]->empty()) {
              ::TimeTool::Fex& fex = *_fex[i];
              fex.reset();
              fex.m_pedestal = _frame[i]->offset();
              fex.analyze(_frame[i]->data(), fifo, 0);
                          
              if (!fex.write_image()) {
                uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
                FrameTrim iter(pdg,fex.src());
                iter.process(&dg->xtc);
              }

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
              _insert_pv(dg, src, 6, fex.sig_roi_sum());

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
      else if (xtc->contains.id()==TypeId::Id_VimbaFrame) {
        for(unsigned i=0; i<_fex.size(); i++)
          if (_fex[i]->m_get_key == xtc->src.phy()) {
            _frame[i]->set_frame(xtc->contains, xtc->payload());
          }
      }
      else if (xtc->contains.id()==TypeId::Id_Frame) {
        for(unsigned i=0; i<_fex.size(); i++)
          if (_fex[i]->m_get_key == xtc->src.phy()) {
            if (xtc->contains.compressed()) {
              _pXtc [i] = Pds::CompressedXtc::uncompress(*xtc);
              _frame[i]->set_frame(xtc->contains, _pXtc[i]->payload());
            }
            else
              _frame[i]->set_frame(xtc->contains, xtc->payload());
          }
      }
      else if (xtc->contains.id()==TypeId::Id_Opal1kConfig) {
        for(unsigned i=0; i<_fex.size(); i++) {
          ::TimeTool::Fex& fex = *_fex[i];
          if (fex.m_get_key == xtc->src.phy()) {
            InDatagram* dg = _dg;
            //const Src& src = fex.src();
            const Src& src = xtc->src;
            _insert_pv(dg, src, 0, fex.base_name()+":AMPL");
            _insert_pv(dg, src, 1, fex.base_name()+":FLTPOS");
            _insert_pv(dg, src, 2, fex.base_name()+":FLTPOS_PS");
            _insert_pv(dg, src, 3, fex.base_name()+":FLTPOSFWHM");
            _insert_pv(dg, src, 4, fex.base_name()+":AMPLNXT");
            _insert_pv(dg, src, 5, fex.base_name()+":REFAMPL");
            _insert_pv(dg, src, 6, fex.base_name()+":SIGROISUM");
            // create frame cache
            _frame[i] = ::TimeTool::FrameCache::instance(xtc->src, xtc->contains, xtc->payload());
          }
        }
      }
      return 1;
    }
  private:
    Appliance&                            _app;
    GenericPool                           _occPool;
    std::vector< ::TimeTool::Fex*       > _fex;
    std::vector< ::TimeTool::FrameCache*> _frame;
    std::vector<PVWriter*               > _pv_writer;
    std::vector<boost::shared_ptr<Pds::Xtc> > _pXtc;
    unsigned  _adjust_n;
    double    _adjust_v;
    InDatagram* _dg;
  };


};


Fex::Fex(const char* fname) :
  ::TimeTool::Fex(fname)
{
}

Fex::~Fex()
{
}

typedef std::map<Pds::Src,ndarray<double,1> > MapType;

static MapType _ref;
static Semaphore _sem(Semaphore::FULL);

//
//  Ideally, each thread's 'm_ref' array would reference the
//  same ndarray, but then I would need to control exclusive
//  access during the reference updates
//
void Fex::_monitor_raw_sig (const ndarray<const double,1>&) 
{
  MapType::iterator it = _ref.find(_src);
  if (it != _ref.end())
    std::copy(it->second.begin(), it->second.end(), m_ref_avg.begin());
}

void Fex::_monitor_ref_sig (const ndarray<const double,1>& ref) 
{
  MapType::iterator it = _ref.find(_src);
  if (it == _ref.end()) {
    ndarray<double,1> a = make_ndarray<double>(ref.size());
    std::copy(ref.begin(), ref.end(), a.begin());
    _sem.take();
    _ref[_src] = a;
    _sem.give();
  }
  else {
    _sem.take();
    psalg::rolling_average(ref, it->second, m_ref_convergence);
    _sem.give();
  }
}
 

static std::vector<Appliance*> _apps; 
const std::vector<Appliance*> apps(Appliance& a)
{
  for(unsigned i=0; i<NWORK_THREADS; i++)
    _apps.push_back(new FexApp("timetool.input",a));
  return _apps;
}
 
TimeToolB::TimeToolB() :
  WorkThreads("ttool", apps(*this)),
  _evr (32)
{
}

TimeToolB::~TimeToolB()
{
  for(unsigned i=0; i<_apps.size(); i++)
    delete _apps[i];
  _apps.clear();
}

InDatagram* TimeToolB::events(InDatagram* dg)
{
  if (dg->datagram().seq.service()==TransitionId::L1Accept) {
    //
    //  Encode the EVR FIFO onto the tail of this datagram
    //
    uint32_t b = (dg->datagram().seq.stamp().fiducials()&0x1f);
    const Xtc& xtc = dg->datagram().xtc;
    uint32_t* v = reinterpret_cast<uint32_t*>(xtc.payload()+xtc.sizeofPayload());
    const std::vector<Pds::EvrData::FIFOEvent>& fifo = _evr[b];
    v[0] = fifo.size();
    for(unsigned i=0; i<fifo.size(); i++)
      v[i+1] = fifo[i].eventCode();
    _evr[b].clear();
  }
  return WorkThreads::events(dg);
}

Occurrence* TimeToolB::occurrences(Occurrence* occ) {
  if (occ->id() == OccurrenceId::EvrCommand) {
    const EvrCommand& cmd = *reinterpret_cast<const EvrCommand*>(occ);
    _evr[(cmd.seq.stamp().fiducials()&0x1f)].push_back(Pds::EvrData::FIFOEvent(0,0,cmd.code));
  }
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new TimeToolB; }

extern "C" void destroy(Appliance* p) { delete p; }
