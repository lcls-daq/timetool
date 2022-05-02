tgtnames    := timetool timetooldb

tgtsrcs_timetool := timetool.cc
tgtlibs_timetool := pds/utility pds/collection pds/service pds/vmon pds/mon pds/xtc
tgtlibs_timetool += timetool/ttevent timetool/ttsvc
tgtlibs_timetool += pdsdata/xtcdata pdsdata/anadata pdsdata/compressdata pdsdata/indexdata pdsdata/psddl_pdsdata psalg/psalg
tgtlibs_timetool += gsl/gsl gsl/gslcblas
tgtslib_timetool := ${USRLIBDIR}/rt ${USRLIBDIR}/pthread
tgtincs_timetool := pdsdata/include psalg/include boost/include ndarray/include

tgtsrcs_timetooldb := timetooldb.cc
tgtlibs_timetooldb := pds/utility pds/collection pds/service pds/vmon pds/mon pds/xtc
tgtlibs_timetooldb += pds/epicstools epics/ca epics/Com
tgtlibs_timetooldb += timetool/tteventdb timetool/tteventepics timetool/ttsvc
tgtlibs_timetooldb += pdsdata/xtcdata pdsdata/anadata pdsdata/compressdata pdsdata/indexdata pdsdata/psddl_pdsdata psalg/psalg
tgtlibs_timetooldb += gsl/gsl gsl/gslcblas
tgtslib_timetooldb := ${USRLIBDIR}/rt ${USRLIBDIR}/pthread
tgtincs_timetooldb := pdsdata/include psalg/include boost/include ndarray/include
