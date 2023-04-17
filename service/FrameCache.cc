#include "FrameCache.hh"

#include "pds/config/Opal1kConfigType.hh"
#include "pds/config/VimbaConfigType.hh"
#include "pds/config/VimbaDataType.hh"

using namespace TimeTool;

FrameCache* FrameCache::instance(Pds::Src src,
                                 Pds::TypeId config_type,
                                 const void* config_payload)
{
  FrameCache* frame = 0;
  switch(config_type.id()) {
  case Pds::TypeId::Id_AlviumConfig:
    switch(config_type.version()) {
    case 1:
      frame = new VimbaFrameCache<Pds::Vimba::AlviumConfigV1, VimbaDataType>(
          src,
          reinterpret_cast<const Pds::Vimba::AlviumConfigV1*>(config_payload));
      break;
    default: break;
    } break;
  case Pds::TypeId::Id_Opal1kConfig:
    frame = new OpalFrameCache<Pds::Camera::FrameV1>(src);
    break;
  default: break;
  }

  return frame;
}

FrameCache::FrameCache(Pds::Src src) :
  _src(src)
{}

const Pds::Src& FrameCache::src() const
{
  return _src;
}

template<class Data>
OpalFrameCache<Data>::OpalFrameCache(Pds::Src src) :
  FrameCache(src),
  _frame(NULL)
{}

template<class Data>
OpalFrameCache<Data>::~OpalFrameCache()
{
  _frame = NULL;
}

template<class Data>
ndarray<const uint16_t, 2> OpalFrameCache<Data>::data() const
{
  if (_frame)
    return _frame->data16();
  else
    return make_ndarray<uint16_t>(0U,0);
}

template<class Data>
void OpalFrameCache<Data>::set_frame(Pds::TypeId data_type,
                                     const void* data_payload)
{
  if ((data_type.id() == static_cast<int>(Data::TypeId)) &&
      (data_type.version() == Data::Version)) {
    _frame = reinterpret_cast<const Data*>(data_payload);
  }
}

template<class Data>
void OpalFrameCache<Data>::clear_frame()
{
  _frame = NULL;
}

template<class Data>
bool OpalFrameCache<Data>::empty() const
{
  return _frame == NULL;
}

template<class Data>
uint32_t OpalFrameCache<Data>::offset() const
{
  return _frame ? _frame->offset() : 0;
}

template<class Cfg, class Data>
VimbaFrameCache<Cfg,Data>::VimbaFrameCache(Pds::Src src, const Cfg* config) :
  FrameCache(src),
  _config_buffer(new char[config->_sizeof()]),
  _config(new (_config_buffer) Cfg(*config)),
  _frame(NULL)
{}

template<class Cfg, class Data>
VimbaFrameCache<Cfg,Data>::~VimbaFrameCache()
{
  _frame = NULL;
  _config = NULL;
  if (_config_buffer) {
    delete[] _config_buffer;
    _config_buffer = NULL;
  }
}

template<class Cfg, class Data>
ndarray<const uint16_t, 2> VimbaFrameCache<Cfg,Data>::data() const
{
  if (_frame)
    return _frame->data(*_config);
  else
    return make_ndarray<uint16_t>(0U,0);
}

template<class Cfg, class Data>
void VimbaFrameCache<Cfg,Data>::set_frame(Pds::TypeId data_type,
                                          const void* data_payload)
{
  if ((data_type.id() == static_cast<int>(Data::TypeId)) &&
      (data_type.version() == Data::Version)) {
    _frame = reinterpret_cast<const Data*>(data_payload);
  }
}

template<class Cfg, class Data>
void VimbaFrameCache<Cfg,Data>::clear_frame()
{
  _frame = NULL;
}

template<class Cfg, class Data>
bool VimbaFrameCache<Cfg,Data>::empty() const
{
  return _frame == NULL;
}

template<class Cfg, class Data>
uint32_t VimbaFrameCache<Cfg,Data>::offset() const
{
  return 0;
}
