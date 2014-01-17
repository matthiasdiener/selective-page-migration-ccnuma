##===- TEST.LoopStructure.Makefile ------------------------*- Makefile -*--===##
#
# This test is used in conjunction with the llvm/utils/NightlyTest* stuff to
# generate information about program status for the nightly report.
#
##===----------------------------------------------------------------------===##

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(PROJ_SRC_ROOT)
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))

REPORT_DEPENDENCIES := $(LOPT)

$(PROGRAMS_TO_TEST:%=test.$(TEST).%): \
test.$(TEST).%: Output/%.$(TEST).report.txt
	@cat $<

# Overall tests: just run subordinate tests
$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt): \
Output/%.$(TEST).report.txt: Output/%.linked.rbc $(LOPT) \
	$(PROJ_SRC_ROOT)/TEST.LoopStructure.Makefile
	$(VERB) $(RM) -f $@
	@echo "--------------------------------------------------------------" >> $@
	@echo ">>> ========= '$(RELDIR)/$*' Program" >> $@
	@echo "--------------------------------------------------------------" >> $@
	@-if test -f $<; then \
		$(LOPT) -mem2reg -instnamer -break-crit-edges \
			 -load DepGraph.so -LoopStructure $< -stats -time-passes 2>> $@; \
	fi
	


