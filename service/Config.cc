#include "Config.hh"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* delim = " \t\n";

template <typename T>
class Result {
public:
  Result(const char* arg) : _arg(arg) {}
public:
  operator T() const;
private:
  const char* _arg;
};

template <typename T>
class ResultSeq {
public:
  ResultSeq(char* arg) : _arg(arg) {}
public:
  operator std::vector<T>() const;
private:
  mutable char* _arg;
};


template <>
Result<double>::operator double() const
{ return strtod(_arg,NULL); }

template <>
Result<int>::operator int() const
{ return strtol(_arg,NULL,0); }

template <>
Result<unsigned>::operator unsigned() const
{ return strtoul(_arg,NULL,0); }

template <>
Result<unsigned long long>::operator unsigned long long() const
{ return strtoull(_arg,NULL,0); }

template <>
Result<std::string>::operator std::string() const
{ return std::string(_arg); }

template <>
Result<bool>::operator bool() const
{ return (_arg[0]=='t' || _arg[0]=='T'); }

template <typename T>
ResultSeq<T>::operator std::vector<T>() const {
  std::vector<T> v;
  const char* p;
  while((p = strtok_r(0,delim,&_arg))) {
    v.push_back(Result<T>(p));
  }
  return v;
}

using namespace TimeTool;

Config::Config(const char* fname) :
  _f(fopen(fname,"r")),
  _ptok(0)
{
}
Config::~Config()
{
  if (_f) fclose(_f);
}

template <typename O>
O Config::config(const std::string& name,
                 const O& def)
{
  O result(def);

  if (!_f) return result;

  rewind(_f);

  size_t sz = 8 * 1024;
  char* linep = (char *)malloc(sz);
  char* _ptok;

  while(getline(&linep, &sz, _f)>0) {
    if (linep[0]=='#')
      continue;
    char* pname = strtok_r(linep,delim,&_ptok);
    if (!pname) continue;

    if (std::string(pname)==name) {
      const char* arg = strtok_r(0,delim,&_ptok);
      result = Result<O>(arg);
      break;
    }
  }

  return result;
}

template <typename O>
std::vector<O> Config::config(const std::string& name,
                              const std::vector<O>& def)
{
  std::vector<O> result(def);

  if (!_f) return result;

  rewind(_f);

  size_t sz = 8 * 1024;
  char* linep = (char *)malloc(sz);
  char* _ptok;

  while(getline(&linep, &sz, _f)>0) {
    if (linep[0]=='#')
      continue;
    char* pname = strtok_r(linep,delim,&_ptok);
    if (!pname) continue;

    if (std::string(pname)==name) {
      result = ResultSeq<O>(_ptok);
      break;
    }
  }

  return result;
}

template bool Config::config<bool>(const std::string& name, const bool& def);
template int Config::config<int>(const std::string& name, const int& def);
template unsigned Config::config<unsigned>(const std::string& name, const unsigned& def);
template double Config::config<double>(const std::string& name, const double& def);
template std::string Config::config<std::string>(const std::string& name, const std::string& def);
template std::vector<double> Config::config<double>(const std::string& name, const std::vector<double>& def);




