#ifndef TimeTool_ConfigCache_hh
#define TimeTool_ConfigCache_hh

#include "pdsdata/xtc/TypeId.hh"

#include <map>

namespace TimeTool {
  class ConfigCache {
  public:
    static ConfigCache* instance(Pds::TypeId config_type,
                                 const void* config_payload);
    virtual ~ConfigCache() {}
    virtual bool data(Pds::TypeId data_type,
                      const void* data_payload) = 0;
    virtual const char* base_name () const = 0;
    virtual bool is_signal        () const = 0;
    virtual double amplitude      () const = 0;
    virtual double position_pixel () const = 0;
    virtual double position_time  () const = 0;
    virtual double position_fwhm  () const = 0;
    virtual double ref_amplitude  () const = 0;
    virtual double nxt_amplitude  () const = 0;
    virtual double signal_integral() const = 0;

  };

  template<class Cfg, class Data>
  class CfgCache : public ConfigCache {
  public:
    CfgCache(const Cfg* config);
    ~CfgCache();
    bool data(Pds::TypeId data_type,
              const void* data_payload);
    const char* base_name() const;
    bool is_signal() const;
    double amplitude() const;
    double position_pixel() const;
    double position_time() const;
    double position_fwhm() const;
    double ref_amplitude() const;
    double nxt_amplitude() const;
    double signal_integral() const;
  private:
    char*       _config_buffer;
    Cfg*        _config;
    const Data* _data;
  };
}

#endif
