tgtnames    := timetool

tgtsrcs_timetool := timetool.cc
tgtlibs_timetool := pds/utility pds/collection pds/service pds/vmon pds/mon pds/xtc
tgtlibs_timetool += timetool/ttevent timetool/ttsvc
tgtlibs_timetool += pdsdata/xtcdata pdsdata/anadata pdsdata/compressdata pdsdata/indexdata pdsdata/psddl_pdsdata pdsalg/pdsalg
tgtslib_timetool := ${USRLIBDIR}/rt ${USRLIBDIR}/pthread 
tgtincs_timetool := pdsdata/include pdsalg/include boost/include ndarray/include 

