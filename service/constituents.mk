libnames := ttsvc

libsrcs_ttsvc := $(filter-out ParabFit.cc, $(wildcard *.cc))
libincs_ttsvc := pdsdata/include
libincs_ttsvc += pdsalg/include ndarray/include
