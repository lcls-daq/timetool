libnames := ami

libsrcs_ami := TimeToolM.cc
libsrcs_ami += $(wildcard ../service/*.cc)
liblibs_ami := pdsalg/pdsalg
libincs_ami := pdsdata/include ndarray/include pdsalg/include
