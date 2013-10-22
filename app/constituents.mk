libnames := ttapp

libsrcs_ttapp := TimeToolA.cc
liblibs_ttapp := timetool/ttsvc pdsalg/pdsalg
libincs_ttapp := epics/include epics/include/os/Linux 
libincs_ttapp += pdsdata/include ndarray/include pdsalg/include
