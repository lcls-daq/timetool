libnames := ttevent tteventdb

libsrcs_ttevent := TimeToolA.cc
liblibs_ttevent += timetool/ttsvc
libincs_ttevent := epics/include epics/include/os/Linux
libincs_ttevent += pdsdata/include ndarray/include boost/include

libsrcs_tteventdb := TimeToolC.cc
liblibs_tteventdb += timetool/ttsvc
libincs_tteventdb := epics/include epics/include/os/Linux
libincs_tteventdb += pdsdata/include ndarray/include boost/include
