libnames := app

libsrcs_app := TimeToolA.cc
libsrcs_app += $(wildcard ../service/*.cc)
libincs_app := epics/include epics/include/os/Linux 
libincs_app += pdsdata/include ndarray/include 
