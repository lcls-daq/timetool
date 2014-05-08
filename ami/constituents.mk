libnames := ami

libsrcs_ami := TimeToolM.cc
libsrcs_ami += $(wildcard ../service/*.cc)
liblibs_ami := psalg/psalg
libincs_ami := pdsdata/include ndarray/include boost/include psalg/include
