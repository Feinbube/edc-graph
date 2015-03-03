
SUBDIR = edc-graph

BUILDDIRS = $(SUBDIR:%=build-%)
CLEANDIRS = $(SUBDIR:%=clean-%)

all: $(BUILDDIRS)
$(SUBDIR): $(BUILDDIRS)
$(BUILDDIRS):
	cd $(@:build-%=%); make

clean: $(CLEANDIRS)
$(CLEANDIRS): 
	cd $(@:clean-%=%); make clean


.PHONY: subdir $(SUBDIR)
.PHONY: subdir $(BUILDDIRS)
.PHONY: subdir $(CLEANDIRS)
.PHONY: all install clean test
