#!/bin/bash

# This file is a part of the TChecker project.
#
# See files AUTHORS and LICENSE for copyright details.

# Check parameters

K=10

function usage() {
    echo "Usage: $0 N";
    echo "       N number of processes";
}

if [ $# -eq 1 ]; then
    N=$1
else
    usage
    exit 1
fi

cat << EOF
//
// System parallel_${N}
//
const int N = ${N};

chan s;

process Processes(int [1,N] pid) {
    clock x;
    state A, B, C;
    init A;
    trans
        A -> B { assign x = 0; },
        B -> C { guard x <= 1; sync s; };
}

system Processes;
EOF
