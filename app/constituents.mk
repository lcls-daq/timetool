libnames := ttapp ttappmt ttappmtdb

libsrcs_ttapp := TimeToolA.cc
liblibs_ttapp := timetool/ttsvc psalg/psalg
liblibs_ttapp += gsl/gsl gsl/gslcblas
libincs_ttapp := epics/include epics/include/os/Linux 
libincs_ttapp += pdsdata/include ndarray/include boost/include psalg/include

libsrcs_ttappmt := TimeToolB.cc
liblibs_ttappmt := timetool/ttsvc pds/client psalg/psalg
liblibs_ttappmt += gsl/gsl gsl/gslcblas
libincs_ttappmt := epics/include epics/include/os/Linux 
libincs_ttappmt += pdsdata/include ndarray/include boost/include psalg/include

libsrcs_ttappmtdb := TimeToolC.cc TimeToolEpics.cc ConfigHandler.cc
liblibs_ttappmtdb := timetool/ttsvc pds/client pds/configdata psalg/psalg
liblibs_ttappmtdb += pds/epicstools epics/ca epics/Com
liblibs_ttappmtdb += gsl/gsl gsl/gslcblas
libincs_ttappmtdb := epics/include epics/include/os/Linux 
libincs_ttappmtdb += pdsdata/include ndarray/include boost/include psalg/include
