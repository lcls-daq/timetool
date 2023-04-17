#include "TimeToolC.hh"
#include "TimeToolEpics.hh"
#include "ConfigHandler.hh"

#include "pds/client/FrameTrim.hh"
#include "pds/service/Task.hh"
#include "pds/service/TaskObject.hh"
#include "pds/service/Routine.hh"
#include "pds/utility/NullServer.hh"
#include "pds/utility/Occurrence.hh"
#include "pds/utility/OccurrenceId.hh"
#include "pds/config/TimeToolConfigType.hh"
#include "pds/config/TimeToolDataType.hh"

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

#include "pdsdata/psddl/epics.ddl.h"

#include "psalg/psalg.h"

#include "timetool/service/Fex.hh"
#include "timetool/service/FrameCache.hh"

#include "cadef.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string>
#include <new>
#include <fstream>
#include <map>

#define NWORK_THREADS 8

using std::string;

typedef Pds::Opal1k::ConfigV1 Opal1kConfig;
typedef Pds::EvrData::DataV3 EvrDataType;
typedef std::vector<TimeTool::FrameCache*> FrameCacheVec;
typedef std::map<Pds::Src, TimeTool::FrameCache*> FrameCacheMap;
typedef FrameCacheMap::iterator FrameCacheIter;

using namespace Pds;

static void copy_projection(ndarray<const int,1> in,
                            ndarray<const int,1> out)
{
  ndarray<int,1> a = make_ndarray<int>(const_cast<int*>(out.data()),out.shape()[0]);
  std::copy(in.begin(),in.end(),a.begin());
}

static void copy_roi(ndarray<const int,2> in,
                     ndarray<const int,2> out)
{
  ndarray<int,2> a = make_ndarray<int>(const_cast<int*>(out.data()),out.shape()[0],out.shape()[1]);
  std::copy(in.begin(),in.end(),a.begin());
}

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
    Fex(const Src&, const TimeToolConfigType&);
    ~Fex();
  public:
    void _monitor_raw_sig      (const ndarray<const double,1>&);
    void _monitor_ref_sig      (const ndarray<const double,1>&);
    void _monitor_raw_sig_full (const ndarray<const double,2>&);
    void _monitor_ref_sig_full (const ndarray<const double,2>&);
    void _write_ref();
    void _enable_write_ref();
  public:
    void reset();
    const TimeToolConfigType& config() const 
    { return *reinterpret_cast<const TimeToolConfigType*>(_config_buffer); }
    TimeToolDataType::EventType event_type() const { return _etype; }
  private:
    char* _config_buffer;
    TimeToolDataType::EventType   _etype;
  };
    
  //
  //  The appliance that runs in each thread
  //
  class FexApp : public Appliance, public XtcIterator {
  public:
    FexApp() {}
    ~FexApp() {}
  public:
    Transition* transitions(Transition* tr) {
      if (tr->id()==TransitionId::Unconfigure) {
        for(unsigned i=0; i<_fex.size(); i++) {
          if (_fex[i]) {
            _fex[i]->_write_ref();
            delete _fex[i];
          }
          if (_frame[i])
            delete _frame[i];
        }
        _fex  .clear();
        _frame.clear();
        for(FrameCacheIter it=_tmp.begin(); it!=_tmp.end(); ++it) {
          if (it->second)
            delete it->second;
        }
        _tmp.clear();
      }
      return tr;
    }
    InDatagram* events(InDatagram* dg) {
      switch(dg->datagram().seq.service()) {
      case TransitionId::L1Accept:
        {
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
            fifo[i] = Pds::EvrData::FIFOEvent(dg->seq.stamp().fiducials(),
                                              dg->seq.stamp().vector(),
                                              v[i+1]);

          for(unsigned i=0; i<_fex.size(); i++) {
            if (_frame[i] && !_frame[i]->empty()) {
              Fex& fex = *_fex[i];
              fex.reset();
              
              fex.m_pedestal = _frame[i]->offset();
              fex.analyze(_frame[i]->data(), fifo, 0);
                          
              // assumes only one fex per event
              if (!fex.write_image()) {
                uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
                FrameTrim iter(pdg,fex.src());
                iter.process(&dg->xtc);
              }

              Damage dmg(fex.status() ? 0x4000 : 0);

              //  Insert the results
              _insert_pv(dg, src, 0, fex.amplitude());
              _insert_pv(dg, src, 1, fex.filtered_position ());
              _insert_pv(dg, src, 2, fex.filtered_pos_ps ());
              _insert_pv(dg, src, 3, fex.filtered_fwhm ());
              _insert_pv(dg, src, 4, fex.next_amplitude());
              _insert_pv(dg, src, 5, fex.ref_amplitude());

              { char* p = new char[TimeToolDataType::_sizeof(fex.config())];
                TimeToolDataType& d = *new (p) TimeToolDataType(fex.event_type(),
                                                                fex.amplitude(),
                                                                fex.filtered_position(),
                                                                fex.filtered_pos_ps(),
                                                                fex.filtered_fwhm(),
                                                                fex.next_amplitude(),
                                                                fex.ref_amplitude());
                if (fex.write_projections()) {
                  if (fex.use_full_roi()) {
                    copy_roi(fex.m_sig_full, d.full_signal   (fex.config()));
                    copy_roi(fex.m_sb_full , d.full_sideband (fex.config()));
                    copy_roi(fex.m_ref_full, d.full_reference(fex.config()));
                  } else {
                    copy_projection(fex.m_sig, d.projected_signal   (fex.config()));
                    copy_projection(fex.m_sb , d.projected_sideband (fex.config()));
                    copy_projection(fex.m_ref, d.projected_reference(fex.config()));
                  }
                }
                Xtc xtc(_timetoolDataType,src);
                xtc.extent += TimeToolDataType::_sizeof(fex.config());
                dg->insert(xtc, p);
                delete[] p;
              }
            }
          }

          break; }
      case TransitionId::Configure:
        _dg = dg;
        iterate(&dg->xtc);
        // clean up tmp frame cache
        for(FrameCacheIter it=_tmp.begin(); it!=_tmp.end(); ++it) {
          if (it->second)
            delete it->second;
        }
        _tmp.clear();
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
      else if (xtc->contains.id()==Pds::TypeId::Id_AlviumConfig ||
               xtc->contains.id()==Pds::TypeId::Id_Opal1kConfig) {
        bool found = false;
        for(unsigned i=0; i<_fex.size(); i++)
          if (_fex[i]->m_get_key == xtc->src.phy()) {
            _frame[i] = ::TimeTool::FrameCache::instance(xtc->src, xtc->contains, xtc->payload());
          }
        if (!found) {
          _tmp[xtc->src] = ::TimeTool::FrameCache::instance(xtc->src, xtc->contains, xtc->payload());
        }
      }

      else if (xtc->contains.id()==TypeId::Id_TimeToolConfig) {
        Fex* fex = new Fex(xtc->src,
                           *reinterpret_cast<const TimeToolConfigType*>(xtc->payload()));
        fex->_enable_write_ref();

        InDatagram* dg = _dg;
        const Src& src = xtc->src;
        _insert_pv(dg, src, 0, fex->base_name()+":AMPL");
        _insert_pv(dg, src, 1, fex->base_name()+":FLTPOS");
        _insert_pv(dg, src, 2, fex->base_name()+":FLTPOS_PS");
        _insert_pv(dg, src, 3, fex->base_name()+":FLTPOSFWHM");
        _insert_pv(dg, src, 4, fex->base_name()+":AMPLNXT");
        _insert_pv(dg, src, 5, fex->base_name()+":REFAMPL");

        _fex  .push_back(fex);
        FrameCacheIter it = _tmp.find(fex->src());
        if (it != _tmp.end()) {
          // add framecache to vector
          _frame.push_back(it->second);
          // remove framecache from tmp map
          _tmp.erase(it);
        } else {
          _frame.push_back(0);
        }
      }
      return 1;
    }
  private:
    std::vector<Fex*> _fex;
    FrameCacheVec     _frame;
    FrameCacheMap     _tmp;
    std::vector<boost::shared_ptr<Pds::Xtc> > _pXtc;
    InDatagram* _dg;
  };
};


Fex::Fex(const Src& src,
         const TimeToolConfigType& cfg) :
  ::TimeTool::Fex(src,cfg,false),
  _config_buffer (new char[cfg._sizeof()])
{
  memcpy(_config_buffer, &cfg, cfg._sizeof());
}

Fex::~Fex()
{
  delete[] _config_buffer;
}

void Fex::reset()
{
  ::TimeTool::Fex::reset(); 
  _etype = TimeToolDataType::Dark;
}

typedef std::map<Pds::Src,ndarray<double,1> > MapType;
typedef std::map<Pds::Src,ndarray<double,2> > FullMapType;

static bool _ref_written=false;
static MapType _ref;
static FullMapType _ref_full;
static Semaphore _sem(Semaphore::FULL);

//
//  Ideally, each thread's 'm_ref' array would reference the
//  same ndarray, but then I would need to control exclusive
//  access during the reference updates
//
void Fex::_monitor_raw_sig (const ndarray<const double,1>& a) 
{
  _etype = TimeToolDataType::Signal;
  MapType::iterator it = _ref.find(_src);
  if (it != _ref.end()) {
    if (m_ref_avg.size()!=it->second.size())
      m_ref_avg = make_ndarray<double>(it->second.size());
    std::copy(it->second.begin(), it->second.end(), m_ref_avg.begin());
  }
}

void Fex::_monitor_ref_sig (const ndarray<const double,1>& ref) 
{
  _etype = TimeToolDataType::Reference; 
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

void Fex::_monitor_raw_sig_full (const ndarray<const double,2>& a)
{
  _etype = TimeToolDataType::Signal;
  FullMapType::iterator it = _ref_full.find(_src);
  if (it != _ref_full.end()) {
    if (m_ref_avg_full.size()!=it->second.size())
      m_ref_avg_full = make_ndarray<double>(it->second.shape()[0],it->second.shape()[1]);
    std::copy(it->second.begin(), it->second.end(), m_ref_avg_full.begin());
  }
}

void Fex::_monitor_ref_sig_full (const ndarray<const double,2>& ref)
{
  _etype = TimeToolDataType::Reference;
  FullMapType::iterator it = _ref_full.find(_src);
  if (it == _ref_full.end()) {
    ndarray<double,2> a = make_ndarray<double>(ref.shape()[0],ref.shape()[1]);
    std::copy(ref.begin(), ref.end(), a.begin());
    _sem.take();
    _ref_full[_src] = a;
    _sem.give();
  }
  else {
    _sem.take();
    psalg::rolling_average(ref, it->second, m_ref_convergence);
    _sem.give();
  }
}
 
void Fex::_write_ref()
{
  char buff[PATH_MAX];
  if (m_use_full_roi) {
    FullMapType::iterator it = _ref_full.find(_src);
    if (it != _ref_full.end()) {
      _sem.take();
      if (!_ref_written) {
        sprintf(buff,"%s/timetool.ref.%08x", _ref_path.c_str(), m_get_key);
        FILE* f = fopen(buff,"w");
        if (f) {
          for(unsigned i=0; i<it->second.shape()[0]; i++)
            for(unsigned j=0; j<it->second.shape()[1]; j++)
              fprintf(f," %f",it->second(i,j));
          fprintf(f,"\n");
          fclose(f);
        }
        _ref_written=true;
      }
      _sem.give();
    }
  } else {
    MapType::iterator it = _ref.find(_src);
    if (it != _ref.end()) {
      _sem.take();
      if (!_ref_written) {
        sprintf(buff,"%s/timetool.ref.%08x", _ref_path.c_str(), m_get_key);
        FILE* f = fopen(buff,"w");
        if (f) {
          for(unsigned i=0; i<it->second.size(); i++)
            fprintf(f," %f",it->second[i]);
          fprintf(f,"\n");
          fclose(f);
        }
        _ref_written=true;
      }
      _sem.give();
    }
  }
}

void Fex::_enable_write_ref()
{
  _sem.take();
  _ref_written=false;
  _sem.give();
}

static std::vector<Appliance*> _apps; 
const std::vector<Appliance*> apps()
{
  for(unsigned i=0; i<NWORK_THREADS; i++)
    _apps.push_back(new FexApp);
  return _apps;
}
 
TimeToolC::TimeToolC() :
  WorkThreads("ttool", apps()),
  _evr       (32),
  _config    (new ::TimeTool::ConfigHandler(*this)),
  _pool      (sizeof(UserMessage),2)
{
  (new TimeToolEpics)->connect(this);
}

TimeToolC::~TimeToolC()
{
  delete _config;
  for(unsigned i=0; i<_apps.size(); i++)
    delete _apps[i];
  _apps.clear();
}

Transition* TimeToolC::transitions(Transition* tr)
{
  _config->transitions(tr);
  return WorkThreads::transitions(tr);
}

InDatagram* TimeToolC::events(InDatagram* dg)
{
  try {
    _config->events(dg);
  } catch (std::string& e) {
    post(new(&_pool) UserMessage(e.c_str()));
    dg->datagram().xtc.damage.increase(Damage::UserDefined);
    return dg;
  }

  switch (dg->datagram().seq.service()) {
  case TransitionId::L1Accept: 
    {
      //
      //  Encode the EVR FIFO onto the tail of this datagram
      //
      //uint32_t b = (dg->datagram().seq.stamp().fiducials()&0x1f);
      uint32_t fid = dg->datagram().seq.stamp().fiducials();
      uint32_t b = (fid&0x1f);
      const Xtc& xtc = dg->datagram().xtc;
      uint32_t* v = reinterpret_cast<uint32_t*>(xtc.payload()+xtc.sizeofPayload());
      const std::vector<Pds::EvrData::FIFOEvent>& fifo = _evr[b];
      unsigned& n = *v;
      n = 0;
      for(unsigned i=0; i<fifo.size(); i++) {
        if (fifo[i].timestampHigh() == fid) {
          v[++n] = fifo[i].eventCode();
        }
        else {
            printf("Dropping fifoEvent [%x] %x/%x/%x at %s line %d\n",
                   fid,
                   fifo[i].timestampHigh(),
                   fifo[i].timestampLow(),
                   fifo[i].eventCode(),
                   __FILE__, __LINE__);
        }
      }
      _evr[b].clear();
    } break;
  default:
    break;
  }
  return WorkThreads::events(dg);
}

Occurrence* TimeToolC::occurrences(Occurrence* occ) {
  if (occ->id() == OccurrenceId::EvrCommand) {
    const EvrCommand& cmd = *reinterpret_cast<const EvrCommand*>(occ);
    _evr[(cmd.seq.stamp().fiducials()&0x1f)].push_back(Pds::EvrData::FIFOEvent(cmd.seq.stamp().fiducials(),0,cmd.code));
  }
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new TimeToolC; }

extern "C" void destroy(Appliance* p) { delete p; }
