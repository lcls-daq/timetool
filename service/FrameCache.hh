#ifndef TimeTool_FrameCache_hh
#define TimeTool_FrameCache_hh

#include "pdsdata/xtc/TypeId.hh"
#include "ndarray/ndarray.h"

namespace TimeTool {
  
  class FrameCache {
  public:
    static FrameCache* instance(Pds::TypeId config_type,
                                const void* config_payload);

    virtual ~FrameCache() {};
    virtual ndarray<const uint16_t, 2> data() const = 0;
    virtual void set_frame(Pds::TypeId data_type,
                           const void* data_payload) = 0;
    virtual void clear_frame() = 0;
    virtual bool empty() = 0;
  };

  template<class Data>
  class OpalFrameCache : public FrameCache {
  public:
    OpalFrameCache();
    ~OpalFrameCache();
    ndarray<const uint16_t, 2> data() const;
    void set_frame(Pds::TypeId data_type,
                   const void* data_payload);
    void clear_frame();
    bool empty();
  private:
    const Data* _frame;
  };

  template<class Cfg, class Data>
  class VimbaFrameCache : public FrameCache {
  public:
    VimbaFrameCache(const Cfg* config);
    ~VimbaFrameCache();
    ndarray<const uint16_t, 2> data() const;
    void set_frame(Pds::TypeId data_type,
                   const void* data_payload);
    void clear_frame();
    bool empty();
  private:
    char*       _config_buffer;
    Cfg*        _config;
    const Data* _frame;
  };
}

#endif
