#ifndef TimeTool_Config_hh
#define TimeTool_Config_hh

#include <string>
#include <list>
#include <vector>

namespace TimeTool {

  class Config {
  public:
    Config(const char* fname);
    ~Config();
  public:
    template <typename O>
    O config(const std::string& name,const O& def);

    template <typename O>
    std::vector<O> config(const std::string& name,const std::vector<O>& def);
  private:
    FILE* _f;
    char* _ptok;
  };
};

#endif
