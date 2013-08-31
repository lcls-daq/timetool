libnames := ami

libsrcs_ami := TimeToolM.cc
libsrcs_ami += $(wildcard ../service/*.cc)
libincs_ami := pdsdata/include ndarray/include 
