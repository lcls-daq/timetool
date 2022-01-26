libnames := ttsvc

libsrcs_ttsvc := $(wildcard *.cc)
libincs_ttsvc := pdsdata/include
libincs_ttsvc += psalg/include ndarray/include boost/include
libincs_ttsvc += gsl/include

special_include_files := Fex.hh

userall: $(RELEASE_DIR)/build/timetool/include/timetool/service/$(special_include_files);

$(RELEASE_DIR)/build/timetool/include/timetool/service/$(special_include_files): $(special_include_files)
	@echo "[I] Target <include>";
	$(quiet)mkdir -p $(RELEASE_DIR)/build/timetool/include/timetool/service
	$(quiet)cp $< $@

