##############################################################################
# Copyright (C) 2006-2013 Netronome Systems, Inc.  All rights reserved.
#
# File:        samples/Makefile
# Description: Makefile to build all the sample applications.
#

#
# Configuration options
#
NS_HOME?=	/opt/netronome
DEBUG?=		1
OPTIM?=		-O2
NMSB_SUPPORT?=	n

CC=		gcc
CXX=		g++
LD=		ld
CFLAGS=		-Wall -W
LDFLAGS=

NFM_DIR?=	$(NS_HOME)
NFD_DIR?=	$(NS_HOME)
NMSB_DIR?=	$(NS_HOME)/nmsb
GERCHR_DIR?=	$(NS_HOME)/gerchr
LDFLAGS+=	-lnfp -lns_armctrl
NMSB_libs=	nmsb nfm
NMSB_defs=	-DNMSB -DNS_PLATFORM
GERCHR_libs=	$(NMSB_libs) gerchr
GERCHR_defs=	$(NMSB_defs) -DGERCHR

#
# Construct CFLAGS/LDFLAGS
#
NFM_LIB:=	$(NFM_DIR)/lib
NFM_INC:=	$(NFM_DIR)/include
NFD_LIB:=	$(NFD_DIR)/lib
NFD_INC:=	$(NFD_DIR)/include
NMSB_LIB:=	$(NMSB_DIR)/lib
NMSB_INC:=	$(NMSB_DIR)/include
GERCHR_LIB:=	$(GERCHR_DIR)/lib
GERCHR_INC:=	$(GERCHR_DIR)/include

CFLAGS+=	-I$(NFM_INC) -I$(NFD_INC)
LDFLAGS+=	-L$(NFM_LIB) -L$(NFD_LIB) -lstdc++
LDFLAGS+=	-Wl,-R$(NFM_LIB) -Wl,-R$(NFD_LIB) $(LF)

ifneq ($(DEBUG),0)
CFLAGS+=	-O0 -g -ggdb -DDEBUG
LDFLAGS+=	-g -ggdb
else
CFLAGS+=	$(OPTIM) -DNDEBUG
endif

ifeq ($(NMSB_SUPPORT),y)
NMSB=		$(NMSB_libs)
CFLAGS+=	$(NMSB_defs) -L$(NMSB_LIB) -I$(NMSB_INC)
LDFLAGS+=	-L$(NMSB_LIB) -Wl,-rpath,$(NMSB_LIB)
endif

ifeq ($(GERCHR_SUPPORT),y)
GERCHR=		$(GERCHR_libs)
CFLAGS+=	$(GERCHR_defs) -L$(NMSB_LIB) -L$(GERCHR_LIB) -I$(NMSB_INC) \
                -I$(GERCHR_INC)
LDFLAGS+=	-L$(NMSB_LIB) -L$(GERCHR_LIB) -Wl,-rpath,$(NMSB_LIB) \
                -Wl,-rpath,$(GERCHR_LIB)
endif

CXXFLAGS=	$(CFLAGS)

# Generic NFM samples
ALL_SAMPLES = \
	nfm_sample_log \
	nfm_sample_packet \
	nfm_sample_flowstats \
	nfm_sample_ntuple_modify \
	nfm_sample_packet_flow_modify \
	nfm_sample_rules_actions \
	nfm_sample_rules_keys   \
	nfm_sample_rules_ports \
	nfm_sample_rules_pass \
	nfm_sample_rules_fill \
	nfm_sample_rules_via_host \
	nfm_sample_rules_via_host_v6 \
	nfm_sample_indtbl \
	nfm_sample_cntr \
	nfm_sample_lb \
	nfm_sample_get_ports \
	nfm_sample_linkstate \
	nfm_sample_monitor \
	nfm_sample_portstats \
	nfm_sample_pcap_record \
	nfm_sample_pcap_playback \
	nfm_sample_pcap_l3_forward \
	nfm_sample_configure_forwarding \
	nfm_sample_l2_static_entry \
	nfm_sample_neigh_entries \
	nfm_sample_bump_port \
	nfm_sample_ifid \
	nfm_sample_dump_ifids \
	nfm_sample_platform \
	nfm_sample_heartbeat \
	nfm_sample_cntr_l2_l3 \
	nfm_sample_nd_adverts \
	nfm_sample_ipsec \
	nfm_sample_ipsec_affinity \
	nfm_sample_ha_flowsync \
	nfm_sample_ha_api

# NMSB specific samples
ifeq ($(NMSB_SUPPORT),y)
ALL_SAMPLES += \
    nfm_sample_ha_nmsb_config \
    nfm_sample_peg35_nmsb_config
endif

LIBS_nfm_sample_log = nfm pthread
LIBS_nfm_sample_packet = nfm
LIBS_nfm_sample_flowstats = nfm
LIBS_nfm_sample_pcap_record = nfm
LIBS_nfm_sample_pcap_playback = nfm ns_msg nfe pcap
LIBS_nfm_sample_pcap_l3_forward = nfm ns_msg nfe pcap
LIBS_nfm_sample_ntuple_modify = nfm ns_msg pthread
LIBS_nfm_sample_packet_flow_modify = nfm ns_msg pthread rt
LIBS_nfm_sample_rules_actions = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_keys = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_ports = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_pass = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_fill = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_via_host = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_rules_via_host_v6 = nfm ns_msg nfe rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_indtbl = nfm ns_msg nfe pthread
LIBS_nfm_sample_cntr = nfm ns_msg
LIBS_nfm_sample_lb = nfm ns_msg pthread
LIBS_nfm_sample_get_ports = nfm ns_msg nfe rt
LIBS_nfm_sample_linkstate = nfm ns_msg nfe pthread rt
LIBS_nfm_sample_monitor = nfm ns_msg nfe pthread rt $(NMSB) $(GERCHR)
LIBS_nfm_sample_monitor_me = nfm $(NMSB) $(GERCHR)
LIBS_nfm_sample_portstats = nfm ns_msg rt pthread $(NMSB) $(GERCHR)
LIBS_nfm_sample_configure_forwarding = nfm
LIBS_nfm_sample_l2_static_entry = nfm ns_msg
LIBS_nfm_sample_neigh_entries = nfm
LIBS_nfm_sample_bump_port = nfm ns_msg nfe pthread
LIBS_nfm_sample_ifid = nfm ns_msg nfe pthread
LIBS_nfm_sample_dump_ifids = nfm ns_msg nfe pthread
LIBS_nfm_sample_platform = nfm ns_msg $(NMSB) $(GERCHR)
LIBS_nfm_sample_cntr_l2_l3 = nfm ns_msg
LIBS_nfm_sample_nd_adverts = nfm
LIBS_nfm_sample_heartbeat = nfm
LIBS_nfm_sample_ipsec = nfm
LIBS_nfm_sample_ipsec_affinity = nfm
LIBS_nfm_sample_ha_flowsync = nfm
LIBS_nfm_sample_ha_api = nfm
LIBS_nfm_sample_ha_nmsb_config = nfm $(NMSB)
LIBS_nfm_sample_peg35_nmsb_config = nfm $(NMSB)

.PHONY : all
all : $(ALL_SAMPLES)

.PHONY : help
help :
	@echo "NFM sample applications build system."
	@echo
	@echo "List of application targets:"
	@$(foreach app,$(ALL_SAMPLES),echo "$(app)";)
	@echo
	@echo "Pseudo targets:"
	@echo "all   - builds all applications listed above."
	@echo "help  - this text."
	@echo "clean - removes compiled binaries."
	@echo
	@echo "Variables to override:"
	@echo "NS_HOME      - Root directory for Netronome software ($(NS_HOME))"
	@echo "NMSB_SUPPORT - Set to 'y' to enable NMSB support ($(NMSB_SUPPORT))"
	@echo "GERCHR_SUPPORT - Set to 'y' to enable GERCHR support ($(GERCHR_SUPPORT))"
	@echo "OPTIM        - Optimization flags ($(OPTIM))"
	@echo "DEBUG        - Debug flags ($(DEBUG))"
	@echo
	@echo "These are derived from NS_HOME but can be overridden:"
	@echo "NFM_DIR      - Root directory for NFM libraries ($(NFM_DIR))"
	@echo "NFD_DIR      - Root directory for NFD libraries ($(NFD_DIR))"
	@echo "NMSB_DIR     - Root directory for NMSB libraries ($(NMSB_DIR))"

.PHONY : clean
clean :
	rm -f $(ALL_SAMPLES)

% : %.c
	if test -f $<; then \
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) $(foreach l,$(LIBS_$@),-l$l); \
	fi

% : %.cpp
	if test -f $<; then \
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(foreach l,$(LIBS_$@),-l$l); \
	fi
