#!/usr/bin/python

# flow_fairness.py
#
# Runs a simulation with objects continually messaging each other.
# The analysis then generates statistics about the actual rates
# achieved and the weights.  The output can be used to generate
# fairness graphs.

import sys
import subprocess
import os.path

# FIXME It would be nice to have a better way of making this script able to find
# other modules in sibling packages
sys.path.insert(0, sys.path[0]+"/..")

import util.stdio
from cluster.config import ClusterConfig
from cluster.sim import ClusterSimSettings,ClusterSim
import flow_fairness

class FlowPairFairness(flow_fairness.FlowFairness):
    def _setup_cluster_sim(self, rate, io):
        self.cs.scenario = 'unreliablehitpoint'

        if self.local: localval = 'true'
        else: localval = 'false'
        self.cs.object_simple='true'
        self.cs.scenario_options = ' '.join(
            ['--num-pings-per-second=' + str(rate),
             '--num-hp-per-second=' + str(1350),#6999
             '--prob-messages-uniform=0.999',
             '--receivers-per-server=1',
             '--num-objects-per-server=20',
             '--hp-size=128',# + str(self.payload_size),
             '--ping-size=1200',# + str(self.payload_size),
             '--local=' + localval,
             ]
            )
        self.cs.odp_flow_scheduler = self.scheme

        if 'object' not in self.cs.traces['simoh']: self.cs.traces['simoh'].append('object')
        if 'ping' not in self.cs.traces['simoh']: self.cs.traces['simoh'].append('ping')
        #if 'message' not in self.cs.traces['all']: self.cs.traces['all'].append('message')

        cluster_sim = ClusterSim(self.cc, self.cs, io=io)
        return cluster_sim


if __name__ == "__main__":
    nss=9
    nobjects = 1500*nss
    packname = '1a_objects.pack'
    numoh = 3

    cc = ClusterConfig()
    cs = ClusterSimSettings(cc, nss, (nss,1), numoh)
    #cs.region_weight = "const";
    #cs.region_weight_options = '--radius=false'
    cs.region_weight_options = '--flatness=.0055'
    cs.debug = True

    cs.valgrind = False
    cs.profile = False
    cs.oprofile = False
    cs.loglevels["oh"]="error";
    cs.loc = 'standard'
    cs.blocksize = 110
    cs.tx_bandwidth = 50000000
    cs.rx_bandwidth = 5000000
    cs.oseg_cache_size=65536;
    cs.oseg_cache_clean_group=25;
    cs.oseg_cache_entry_lifetime= "1000s"
    cs.oseg_lookup_queue_size = 50; #for now

    # Use pack across multiple ohs
    cs.num_random_objects = 0
    cs.num_pack_objects = nobjects / cs.num_oh
    cs.object_pack = packname
    cs.pack_dump = True

    cs.object_connect_phase = '20s'

    cs.object_static = 'static'
    cs.object_query_frac = 0.0

    cs.duration = '256s'
    rates = sys.argv[1:]
    plan = FlowPairFairness(cc, cs, scheme='csfq', payload=1024)
    for rate in rates:
        plan.run(rate)
        plan.analysis()
    plan.graph()
