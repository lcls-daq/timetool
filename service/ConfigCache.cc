#include "ConfigCache.hh"

#include "pds/config/TimeToolConfigType.hh"

#include <new>

typedef Pds::TimeTool::ConfigV1 CfgTTV1;
typedef Pds::TimeTool::ConfigV2 CfgTTV2;
typedef Pds::TimeTool::ConfigV3 CfgTTV3;

typedef Pds::TimeTool::DataV1 DataTTV1;
typedef Pds::TimeTool::DataV2 DataTTV2;
typedef Pds::TimeTool::DataV3 DataTTV3;

static double calc_signal_integral(const CfgTTV1& cfg, const DataTTV1& data)
{
  double result = 0.0;
  if (cfg.write_projections()) {
    ndarray<const int32_t,1> sig = data.projected_signal(cfg);
    for(const int32_t* a=sig.begin(); a!=sig.end(); a++)
      result += *a;
  }
  return result;
}

static double calc_signal_integral(const CfgTTV2& cfg, const DataTTV2& data)
{
  double result = 0.0;
  if (cfg.write_projections()) {
    ndarray<const int32_t,1> sig = data.projected_signal(cfg);
    for(const int32_t* a=sig.begin(); a!=sig.end(); a++)
      result += *a;
  }
  return result;
}

static double calc_signal_integral(const CfgTTV3& cfg, const DataTTV3& data)
{
  double result = 0.0;
  if (cfg.write_projections()) {
    if (cfg.use_full_roi()) {
      ndarray<const int32_t,2> sig = data.full_signal(cfg);
      for(const int32_t* a=sig.begin(); a!=sig.end(); a++)
        result += *a;
    } else {
      ndarray<const int32_t,1> sig = data.projected_signal(cfg);
      for(const int32_t* a=sig.begin(); a!=sig.end(); a++)
        result += *a;
    }
  }

  return result;
}

using namespace TimeTool;

ConfigCache* ConfigCache::instance(Pds::TypeId config_type,
                                   const void* config_payload)
{
  ConfigCache* cache = 0;
  switch(config_type.id()) {
  case Pds::TypeId::Id_TimeToolConfig:
    switch(config_type.version()) {
    case 1:
      cache = new CfgCache<CfgTTV1,DataTTV1>(
          reinterpret_cast<const CfgTTV1*>(config_payload));
      break;
    case 2:
      cache = new CfgCache<CfgTTV2,DataTTV2>(
          reinterpret_cast<const CfgTTV2*>(config_payload));
      break;
    case 3:
      cache = new CfgCache<CfgTTV3,DataTTV3>(
          reinterpret_cast<const CfgTTV3*>(config_payload));
      break;
    default: break;
    } break;
  default: break;
  }

  return cache;
}

template<class Cfg, class Data>
CfgCache<Cfg,Data>::CfgCache(const Cfg* config) :
  _config_buffer(new char[config->_sizeof()]),
  _config(new (_config_buffer) Cfg(*config)),
  _data(NULL)
{}

template<class Cfg, class Data>
CfgCache<Cfg,Data>::~CfgCache()
{
  _data = NULL;
  _config = NULL;
  if (_config_buffer) {
    delete[] _config_buffer;
    _config_buffer = NULL;
  }
}

template<class Cfg, class Data>
bool CfgCache<Cfg,Data>::data(Pds::TypeId data_type,
                              const void* data_payload)
{
  if (((uint32_t) data_type.id() == Data::TypeId) && (data_type.version() == Data::Version)) {
    _data = reinterpret_cast<const Data*>(data_payload);
    return true;
  } else {
    _data = NULL;
    return false;
  }
}

template<class Cfg, class Data>
const char* CfgCache<Cfg,Data>::base_name() const
{
  return _config->base_name();
}

template<class Cfg, class Data>
bool CfgCache<Cfg,Data>::is_signal() const
{
  return _data->event_type() == Data::Signal;
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::amplitude() const
{
  return _data->amplitude();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::position_pixel() const
{
  return _data->position_pixel();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::position_time() const
{
  return _data->position_time();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::position_fwhm() const
{
  return _data->position_fwhm();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::ref_amplitude() const
{
  return _data->ref_amplitude();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::nxt_amplitude() const
{
  return _data->nxt_amplitude();
}

template<class Cfg, class Data>
double CfgCache<Cfg,Data>::signal_integral() const
{
  return calc_signal_integral(*_config, *_data);
}
