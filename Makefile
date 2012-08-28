FEOSMK = $(FEOSSDK)/mk

MANIFEST    := package.manifest
PACKAGENAME := soundstream

all:
	@$(MAKE) --no-print-directory -C arm7
	@$(MAKE) --no-print-directory -f mainlib.mk

clean:
	@$(MAKE) --no-print-directory -C arm7 clean
	@$(MAKE) --no-print-directory -f mainlib.mk clean

install:
	@$(MAKE) --no-print-directory -C arm7 install
	@$(MAKE) --no-print-directory -f mainlib.mk install
	
include $(FEOSMK)/packagetop.mk
