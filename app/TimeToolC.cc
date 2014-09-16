#include "TimeToolC.hh"
#include "ConfigHandler.hh"

#include "pds/client/FrameTrim.hh"
#include "pds/service/Task.hh"
#include "pds/service/TaskObject.hh"
#include "pds/service/Routine.hh"
#include "pds/utility/NullServer.hh"
#include "pds/utility/Occurrence.hh"
#include "pds/utility/OccurrenceId.hh"
#include "pds/config/TimeToolConfigType.hh"

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

#include "pds/epicstools/PVWriter.hh"
#include "timetool/service/Fex.hh"

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

using namespace Pds;
using Pds_Epics::PVWriter;

static void copy_projection(ndarray<const int,1> in,
			    ndarray<const int,1> out)
{
  ndarray<int,1> a = make_ndarray<int>(const_cast<int*>(out.data()),out.shape()[0]);
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
    void _monitor_raw_sig (const ndarray<const double,1>&);
    void _monitor_ref_sig (const ndarray<const double,1>&);
  public:
    void reset();
    const TimeToolConfigType& config() const 
    { return *reinterpret_cast<const TimeToolConfigType*>(_config_buffer); }
    TimeTool::DataV1::EventType event_type() const { return _etype; }
  private:
    char* _config_buffer;
    TimeTool::DataV1::EventType   _etype;
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
	  delete _fex[i];
	  delete _pvwri[i];
	}
	_fex  .clear();
	_frame.clear();
	_pvwri.clear();
      }
      return tr;
    }
    InDatagram* events(InDatagram* dg) {
      switch(dg->datagram().seq.service()) {
      case TransitionId::L1Accept:
        {
          for(unsigned i=0; i<_fex.size(); i++)
            _frame[i] = 0;
          
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
            if (_frame[i]) {
              Fex& fex = *_fex[i];
              fex.reset();
              
              fex.analyze(_frame[i]->data16(), fifo, 0);
                          
              // assumes only one fex per event
              if (!fex.write_image()) {
                uint32_t* pdg = reinterpret_cast<uint32_t*>(&dg->xtc);
                FrameTrim iter(pdg,fex.src());
                iter.process(&dg->xtc);
              }

              Damage dmg(fex.status() ? 0x4000 : 0);

	      if (_pvwri[i]->connected()) {
		double* v = reinterpret_cast<double*>(_pvwri[i]->data());
		v[0] = fex.amplitude();
		v[1] = fex.filtered_position();
		v[2] = fex.filtered_pos_ps();
		v[3] = fex.filtered_fwhm();
		v[4] = fex.next_amplitude();
		v[5] = fex.ref_amplitude();
		_pvwri[i]->put(); 
	      }

              //  Insert the results
              _insert_pv(dg, src, 0, fex.amplitude());
              _insert_pv(dg, src, 1, fex.filtered_position ());
              _insert_pv(dg, src, 2, fex.filtered_pos_ps ());
              _insert_pv(dg, src, 3, fex.filtered_fwhm ());
              _insert_pv(dg, src, 4, fex.next_amplitude());
              _insert_pv(dg, src, 5, fex.ref_amplitude());

	      { char* p = new char[TimeTool::DataV1::_sizeof(fex.config())];
		TimeTool::DataV1& d = *new (p) TimeTool::DataV1(fex.event_type(),
								fex.amplitude(),
								fex.filtered_position(),
								fex.filtered_pos_ps(),
								fex.filtered_fwhm(),
								fex.next_amplitude(),
								fex.ref_amplitude());
		if (fex.write_projections()) {
		  copy_projection(fex.m_sig, d.projected_signal  (fex.config()));
		  copy_projection(fex.m_sb , d.projected_sideband(fex.config()));
		}
		Xtc xtc(TypeId(TypeId::Id_TimeToolData,1),src);
		xtc.extent += TimeTool::DataV1::_sizeof(fex.config());
		dg->insert(xtc, p);
		delete[] p;
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
      else if (xtc->contains.id()==TypeId::Id_TimeToolConfig) {
	Fex& fex = *new Fex(xtc->src,
			    *reinterpret_cast<const TimeToolConfigType*>(xtc->payload()));
	InDatagram* dg = _dg;
	const Src& src = xtc->src;
	_insert_pv(dg, src, 0, fex.base_name()+":AMPL");
	_insert_pv(dg, src, 1, fex.base_name()+":FLTPOS");
	_insert_pv(dg, src, 2, fex.base_name()+":FLTPOS_PS");
	_insert_pv(dg, src, 3, fex.base_name()+":FLTPOSFWHM");
	_insert_pv(dg, src, 4, fex.base_name()+":AMPLNXT");
	_insert_pv(dg, src, 5, fex.base_name()+":REFAMPL");

	_fex  .push_back(&fex);
	_frame.push_back(0);
	_pvwri.push_back(new PVWriter((fex.base_name()+":ARRAY").c_str()));
      }
      return 1;
    }
  private:
    std::vector<Fex*>                   _fex;
    std::vector<const Camera::FrameV1*> _frame;
    std::vector<PVWriter*>              _pvwri;
    std::vector<boost::shared_ptr<Pds::Xtc> > _pXtc;
    InDatagram* _dg;
  };
};


Fex::Fex(const Src& src,
	 const TimeToolConfigType& cfg) :
  ::TimeTool::Fex(src,cfg),
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
  _etype = TimeTool::DataV1::Dark;
}

typedef std::map<Pds::Src,ndarray<double,1> > MapType;

static MapType _ref;
static Semaphore _sem(Semaphore::FULL);

//
//  Ideally, each thread's 'm_ref' array would reference the
//  same ndarray, but then I would need to control exclusive
//  access during the reference updates
//
void Fex::_monitor_raw_sig (const ndarray<const double,1>& a) 
{
  _etype = TimeTool::DataV1::Signal;
  MapType::iterator it = _ref.find(_src);
  if (it != _ref.end()) {
    if (m_ref.size()!=it->second.size())
      m_ref = make_ndarray<double>(it->second.size());
    std::copy(it->second.begin(), it->second.end(), m_ref.begin());
  }
}

void Fex::_monitor_ref_sig (const ndarray<const double,1>& ref) 
{
  _etype = TimeTool::DataV1::Reference; 
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
const std::vector<Appliance*> apps()
{
  for(unsigned i=0; i<NWORK_THREADS; i++)
    _apps.push_back(new FexApp);
  return _apps;
}
 
TimeToolC::TimeToolC() :
  WorkThreads("ttool", apps()),
  _evr       (32),
  _config    (new TimeTool::ConfigHandler(*this))
{
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
  _config->events(dg);
  switch (dg->datagram().seq.service()) {
  case TransitionId::L1Accept: 
    {
      //
      //  Encode the EVR FIFO onto the tail of this datagram
      //
      uint32_t b = (dg->datagram().seq.stamp().vector()&0x1f);
      const Xtc& xtc = dg->datagram().xtc;
      uint32_t* v = reinterpret_cast<uint32_t*>(xtc.payload()+xtc.sizeofPayload());
      const std::vector<Pds::EvrData::FIFOEvent>& fifo = _evr[b];
      v[0] = fifo.size();
      for(unsigned i=0; i<fifo.size(); i++)
	v[i+1] = fifo[i].eventCode();
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
    _evr[(cmd.seq.stamp().vector()&0x1f)].push_back(Pds::EvrData::FIFOEvent(0,0,cmd.code));
  }
  return occ;
}

//
//  Plug-in module creator
//

extern "C" Appliance* create() { return new TimeToolC; }

extern "C" void destroy(Appliance* p) { delete p; }
