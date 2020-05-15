UK_ROOT ?= $(PWD)/../../unikraft_dpdk
UK_LIBS ?= $(PWD)/../../libs
LIBS :=
all:
	@$(MAKE) -C $(UK_ROOT) A=$(PWD) L=$(LIBS)

$(MAKECMDGOALS):
	@$(MAKE) -C $(UK_ROOT) A=$(PWD) L=$(LIBS) $(MAKECMDGOALS)
