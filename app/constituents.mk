libnames := app

libsrcs_app := TimeToolA.cc
libsrcs_app += $(wildcard ../service/*.cc)
liblibs_app := pdsalg/pdsalg
libincs_app := epics/include epics/include/os/Linux 
libincs_app += pdsdata/include ndarray/include pdsalg/include
