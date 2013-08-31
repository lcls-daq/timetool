libnames := event

libsrcs_event := $(wildcard *.cc)
libsrcs_event += $(wildcard ../service/*.cc)
libincs_event := epics/include epics/include/os/Linux
libincs_event += pdsdata/include ndarray/include 