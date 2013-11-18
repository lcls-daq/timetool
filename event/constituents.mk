libnames := ttevent

libsrcs_ttevent := $(wildcard *.cc)
liblibs_ttevent += timetool/ttsvc
libincs_ttevent := epics/include epics/include/os/Linux
libincs_ttevent += pdsdata/include ndarray/include boost/include 