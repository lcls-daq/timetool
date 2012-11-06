libnames := app

libsrcs_app := $(wildcard *.cc)
libsrcs_app += $(wildcard ../service/*.cc)
libincs_app := epics/include epics/include/os/Linux