#!/bin/bash
#
#
# Copyright (C) 2008-2011 Netronome Systems, Inc.  All rights reserved.
#
# File:        nfm_sample_load_balance.sh
#
# Description: A simple wrapper shell script showing how to use other
#              NFM samples and command for load balancing.  Use with
#              caution.  This will flush rules and zero out any previous
#              load balancing state.

# LGID to configure (possible values 0-23)
LGID=0

# Configuration -- 4 destids per card (4 listener instances)
#HOST_ID_LIST=1,2,3,4

# Configuration -- non NMSB case
# 3 destids per card (3 listener instances)
#HOST_ID_LIST=1,2,3

# Configuration -- NMSB case
# 6 destids per card (6 listener instances)
# testpcap -i +Pnfe0.1:1.1
# testpcap -i +Pnfe0.2:1.2
# ... and so on till
# testpcap -i +Pnfe0.6:1.6
HOST_ID_LIST=1,2,3,4,5,6

# Compile the samples if you need to
# make -C /opt/netronome/nfm/samples

# Set the PATH
export PATH="/opt/netronome/nfm/bin:/opt/netronome/nfm/samples:${PATH}"

# Make sure we can find the NFM libraries
export LD_LIBRARY_PATH="/opt/netronome/nfm/lib"

#CARDS_LIST="0"
# Automatically calculated
CARDS_LIST="$(seq 0 $(echo $(/opt/netronome/nfd/bin/lsnfe | wc -l) - 1 | bc))"

for cardid in ${CARDS_LIST}; do

    # Remove previous rules state
    echo "=== Flushing rules"
    echo "rules --flush -C $cardid"
    rules --flush -C $cardid

    # Add a rule for PASSing the ARP traffic.  This can instead be
    # directed to a particular destination ID on the host if desired.
    # Note that ARP traffic is not load-balanced since it is non-flow.
    # Please see the User's guide for more information on Load
    # balancing.  See rules --samples and rules --help for more help
    # on rules command.
    echo "=== Adding ARP rule to pass the traffic"
    echo "rules --add --name=rule_ARP --common_action --flow_timeout=30s \
	--priority=0 --flow_from_both --action=PASS --key --etype=ETHERTYPE_ARP -C $cardid -M"
    rules --add --name=rule_ARP --common_action --flow_timeout=30s \
	--priority=0 --flow_from_both --action=PASS --key --etype=ETHERTYPE_ARP -C $cardid -M

    # Zero out the LGID table
    echo "=== Zeroing the LGID table"
    echo "nfm_sample_lb -d $cardid -c"
    nfm_sample_lb -d $cardid -c

    # Setup the LBID to destination mapping
    echo "=== Setting up the LBID --> host destination id mapping"
    echo "nfm_sample_lb -d $cardid -S ${LGID}:${HOST_ID_LIST}"
    nfm_sample_lb -d $cardid -S ${LGID}:${HOST_ID_LIST}

    # Display the LB table
    echo "=== Displaying the LGID table"
    echo "nfm_sample_lb -d $cardid -p"
    nfm_sample_lb -d $cardid -p

    # Associate the above mapping with a rule
    echo "=== Adding a rule to use the above LGID ${LGID}"
    echo "rules -A -n lb_test_$cardid -p 0 -k -b --lgid=${LGID} -a VIA_HOST -C $cardid -M"
    rules -A -n lb_test_$cardid -p 0 -k -b --lgid=${LGID} -a VIA_HOST -C $cardid -M

    echo "=== Done."
    echo
    echo "You are now ready to run a custom NFM applications or libpcap based applications."

done
