##===- TEST.nightly.Makefile ------------------------------*- Makefile -*--===##
#
# This test is used in conjunction with the llvm/utils/NightlyTest* stuff to
# generate information about program status for the nightly report.
#
##===----------------------------------------------------------------------===##

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(PROJ_SRC_ROOT)
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))

PROCESSLOOPS := $(PROGDIR)/processLoops.sh

REPORTS_TO_GEN := nat instr
REPORT_DEPENDENCIES := $(LOPT)
ifndef DISABLE_LLC
REPORTS_TO_GEN +=  llc compile
REPORT_DEPENDENCIES += $(LLC) $(LOPT)
endif
REPORTS_SUFFIX := $(addsuffix .report.txt, $(REPORTS_TO_GEN))

#Instrumentation Step
ifdef NO_INSTRUMENT
$(PROGRAMS_TO_TEST:%=Output/%.linked.rbc.instr.bc):  \
Output/%.linked.rbc.instr.bc: Output/%.linked.rbc $(LOPT)
	@-$(LOPT) -mem2reg -break-crit-edges -instnamer -stats -time-passes \
		$< -o $@		
else
$(PROGRAMS_TO_TEST:%=Output/%.linked.rbc.instr.bc):  \
Output/%.linked.rbc.instr.bc: Output/%.linked.rbc $(LOPT)
	@-$(LOPT) -mem2reg -break-crit-edges -instnamer -stats -time-passes \
		-load DepGraph.so -loop-normalizer -tc-profiler $< -o $<.tmp \
		2>Output/$*.instrumentation.stats
	clang -S -emit-llvm $(PROGDIR)/InstrumentationLibrariesToLink/TcProfilerLinkedLibrary.c -o $(PROGDIR)/InstrumentationLibrariesToLink/TcProfilerLinkedLibrary.bc
	$(RUNSAFELY) $(STDIN_FILENAME) Output/$*.linked.rbc.instr.bc.info llvm-link $(PROGDIR)/InstrumentationLibrariesToLink/TcProfilerLinkedLibrary.bc $<.tmp -o=$@
endif

#Output of the Instrumentation step
$(PROGRAMS_TO_TEST:%=Output/%.instrumentation.stats): \
Output/%.instrumentation.stats: Output/%.linked.rbc.instr.bc
	@

# Generate a .o file from the llvm.bc file with the integrated assembler.
$(PROGRAMS_TO_TEST:%=Output/%.llc-instr.o): \
Output/%.llc-instr.o: Output/%.linked.rbc.instr.bc $(LLC)
	$(VERB) $(RM) -f $(CURDIR)/$@.info
	$(RUNSAFELYLOCAL) /dev/null $@.llc \
	  $(LLC) $(LLCFLAGS) -filetype=obj $< -o $@ -info-output-file=$(CURDIR)/$@.info $(STATS)

#
# Compile a linked program to machine code with LLC.
#
$(PROGRAMS_TO_TEST:%=Output/%.llc-instr.s): \
Output/%.llc-instr.s: Output/%.linked.rbc.instr.bc $(LLC)
	$(VERB) $(RM) -f $(CURDIR)/$@.info
	$(RUNSAFELYLOCAL) /dev/null $@.llc \
	  $(LLC) $(LLCFLAGS) $(LLCOPTION) $< -o $@ -info-output-file=$(CURDIR)/$@.info $(STATS)


ifdef TEST_INTEGRATED_ASSEMBLER

# Link an LLVM-linked program using the system linker.
$(PROGRAMS_TO_TEST:%=Output/%.llc-instr): \
Output/%.llc-instr: Output/%.llc-instr.o
	-$(PROGRAMLD) $< -o $@ $(LLCLIBS) $(LLCASSEMBLERFLAGS) $(X_TARGET_FLAGS) $(LDFLAGS)

else

# Assemble/Link an LLVM-linked program using the system assembler and linker.
#
$(PROGRAMS_TO_TEST:%=Output/%.llc-instr): \
Output/%.llc-instr: Output/%.llc-instr.s
	-$(PROGRAMLD) $< -o $@ $(LLCLIBS) $(LLCASSEMBLERFLAGS) $(X_TARGET_FLAGS) $(LDFLAGS)
	
endif

#Here we run the program
$(PROGRAMS_TO_TEST:%=Output/%.out-llc-instr): \
Output/%.out-llc-instr: Output/%.llc-instr
	$(VERB) $(RM) -f loops.out
	$(RUNSAFELY) $(STDIN_FILENAME) $@ $< $(RUN_OPTIONS) 
	$(RUNSAFELY) $(STDIN_FILENAME) Output/$*.loops.out.info mv loops.out Output/$*.loops.out
	$(RUNSAFELY) $(STDIN_FILENAME) Output/$*.out-llc-instr-loops /bin/bash $(PROCESSLOOPS) Output/$*.loops.out 
ifdef PROGRAM_OUTPUT_FILTER
	$(PROGRAM_OUTPUT_FILTER) $@
endif


#file containing loops separated by program
$(PROGRAMS_TO_TEST:%=Output/%.loops.out): \
Output/%.loops.out: Output/%.out-llc-instr
	@

#file containing loops separated by program
$(PROGRAMS_TO_TEST:%=Output/%.out-llc-instr-loops): \
Output/%.out-llc-instr-loops: Output/%.out-llc-instr
	@


$(PROGRAMS_TO_TEST:%=Output/%.instr-output): \
Output/%.instr-output: Output/%.out-llc-instr
	echo "" > $@

#Here we extract the information added by our profiler and provide a clean output
$(PROGRAMS_TO_TEST:%=Output/%.clean-out-llc-instr): \
Output/%.clean-out-llc-instr: Output/%.out-llc-instr Output/%.instr-output
	bash $(PROGDIR)/TripCountExtractor.sh Output/$*.out-llc-instr Output/$*.instr-output $@

$(PROGRAMS_TO_TEST:%=Output/%.diff-llc-instr): \
Output/%.diff-llc-instr: Output/%.out-nat Output/%.out-llc-instr
	-$(DIFFPROG) llc-instr $* $(HIDEDIFF)

$(PROGRAMS_TO_TEST:%=Output/%.exe-llc-instr): \
Output/%.exe-llc-instr: Output/%.diff-llc-instr
	-rm -f $@
	-cp $< $@


# Instrumented tests
$(PROGRAMS_TO_TEST:%=Output/%.nightly.instr.report.txt): \
Output/%.nightly.instr.report.txt: Output/%.exe-llc-instr Output/%.out-llc-instr-loops Output/%.loops.out Output/%.instrumentation.stats
	@echo > $@
	@-if test -f Output/$*.exe-llc-instr; then \
	  head -n 100 Output/$*.exe-llc-instr >> $@; \
	  echo "TEST-PASS: instr $(RELDIR)/$*" >> $@;\
	  printf "TEST-RESULT-instr: " >> $@;\
	  grep "Total Execution Time" Output/$*.llc-instr.s.info | tail -n 1 >> $@;\
	  printf "TEST-RESULT-instr-time: " >> $@;\
	  grep "^user" Output/$*.out-llc-instr.time >> $@;\
	  echo >> $@;\
	  cat Output/$*.out-llc-instr-loops >> $@;\
	  echo >> $@;\
	  cat Output/$*.instrumentation.stats >> $@;\
	else  \
	  echo "TEST-FAIL: instr $(RELDIR)/$*" >> $@;\
	fi
##===----------------------------------------------------------------------===##
##===   End of custom stuff
##===----------------------------------------------------------------------===##


# Compilation tests
$(PROGRAMS_TO_TEST:%=Output/%.nightly.compile.report.txt): \
Output/%.nightly.compile.report.txt: Output/%.llvm.bc
	@echo > $@
	@-if test -f Output/$*.linked.bc.info; then \
	  echo "TEST-PASS: compile $(RELDIR)/$*" >> $@;\
	  printf "TEST-RESULT-compile: " >> $@;\
	  grep "Total Execution Time" Output/$*.linked.bc.info | tail -n 1 >> $@;\
	  echo >> $@;\
	  printf "TEST-RESULT-compile: " >> $@;\
	  wc -c $< >> $@;\
	  echo >> $@;\
	else \
	  echo "TEST-FAIL: compile $(RELDIR)/$*" >> $@;\
	fi

# NAT tests
$(PROGRAMS_TO_TEST:%=Output/%.nightly.nat.report.txt): \
Output/%.nightly.nat.report.txt: Output/%.out-nat
	@echo > $@
	@printf "TEST-RESULT-nat-time: " >> $@
	-grep "^user" Output/$*.out-nat.time >> $@

# LLC tests
$(PROGRAMS_TO_TEST:%=Output/%.nightly.llc.report.txt): \
Output/%.nightly.llc.report.txt: Output/%.exe-llc
	@echo > $@
	@-if test -f Output/$*.exe-llc; then \
	  head -n 100 Output/$*.exe-llc >> $@; \
	  echo "TEST-PASS: llc $(RELDIR)/$*" >> $@;\
	  printf "TEST-RESULT-llc: " >> $@;\
	  grep "Total Execution Time" Output/$*.llc.s.info | tail -n 1 >> $@;\
	  printf "TEST-RESULT-llc-time: " >> $@;\
	  grep "^user" Output/$*.out-llc.time >> $@;\
	  echo >> $@;\
	else  \
	  echo "TEST-FAIL: llc $(RELDIR)/$*" >> $@;\
	fi


# Overall tests: just run subordinate tests
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: $(addprefix Output/%.nightly., $(REPORTS_SUFFIX))
	$(VERB) $(RM) -f $@
	@echo "---------------------------------------------------------------" >> $@
	@echo ">>> ========= '$(RELDIR)/$*' Program" >> $@
	@echo "---------------------------------------------------------------" >> $@
	-cat $(addprefix Output/$*.nightly., $(REPORTS_SUFFIX)) >> $@


$(PROGRAMS_TO_TEST:%=test.$(TEST).%): \
test.$(TEST).%: Output/%.$(TEST).report.txt
	@-cat $<

$(PROGRAMS_TO_TEST:%=build.$(TEST).%): \
build.$(TEST).%: Output/%.llc
	@echo "Finished Building: $<"
