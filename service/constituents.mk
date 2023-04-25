libnames := ttsvc

libsrcs_ttsvc := $(wildcard *.cc)
libincs_ttsvc := pdsdata/include
libincs_ttsvc += psalg/include ndarray/include boost/include
libincs_ttsvc += gsl/include

special_include_files := Fex.hh FrameCache.hh
special_include_files := $(patsubst %,$(RELEASE_DIR)/build/timetool/include/timetool/service/%,$(special_include_files))

userall: $(special_include_files)

$(RELEASE_DIR)/build/timetool/include/timetool/service/%.hh: %.hh
	@echo "[I] Target <include>";
	$(quiet)mkdir -p $(RELEASE_DIR)/build/timetool/include/timetool/service
	$(quiet)cp $< $@

