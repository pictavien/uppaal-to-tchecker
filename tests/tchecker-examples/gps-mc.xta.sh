#!/bin/bash

# This file is a part of the TChecker project.
#
# See files AUTHORS and LICENSE for copyright details.

# Generates a TChecker model for the GPS-MC example, inspired from:
# "Component-based Abstraction Refinement for Timed Controller Synthesis"
# Hans-Jörg Peter and Robert Mattmüller, RTSS 2009

# Checks command line arguments
if [ ! $# -eq 4 ];
then
    echo "Usage: $0 <# stations> <# sub stations> <processing time> <timeout>"
    exit 0;
fi

NSTATIONS=$1
NSUBSTATIONS=$2
PROCTIME=$3
TIMEOUT=$4

cat << EOF
//
// System gps_mc_${NSTATIONS}_${NSUBSTATIONS}_${PROCTIME}_${TIMEOUT}
//

const int NSTATIONS = ${NSTATIONS};
const int NSUBSTATIONS = ${NSUBSTATIONS};
const int PROCTIME = ${PROCTIME};
const int TIMEOUT = ${TIMEOUT};

typedef int [1,NSTATIONS] station_id_t;
typedef int [1,NSUBSTATIONS+1] substation_id_t;

chan finished[NSTATIONS+1];
chan start[NSTATIONS][substation_id_t];

process Station(const station_id_t i) {
    state idle, start_, processing, done;
    commit start_, done;
    init idle;
    trans
        idle -> start_ { sync finished[i-1]?; },
        start_ -> processing { sync start[i][1]!; },
        processing -> done { sync start[i][NSUBSTATIONS+1]?; },
        done -> idle { sync finished[i]!; };

}

process SubStation(const station_id_t i, const int [1,NSUBSTATIONS] j) {
    clock x;
    state idle,
        processing { x <= PROCTIME };
    init idle;
    trans
        idle -> processing { sync start[i][j]?; assign x = 0; },
        processing -> idle { guard x <= PROCTIME; sync start[i][j+1]!; };
}

process Property() {
    clock z;
    state idle, error, finished_, ok;
    init idle;
    trans
        idle -> finished_ { sync finished[0]!; assign z = 0; },
        finished_ -> error { guard z > TIMEOUT; },
        finished_ -> ok { guard z <= TIMEOUT; sync finished[NSTATIONS]?; },
        ok -> idle {};
}

system Station, SubStation, Property;
EOF

exit
# Model

for i in `seq 0 $NSTATIONS`; do
    echo "event:finished$i"
done

for j in `seq 1 $((NSUBSTATIONS+1))`; do
    echo "event:start$j"
done
echo ""

# Processes

for i in `seq 1 $NSTATIONS`; do
    echo "# Station $i
process:S${i}
location:S${i}:idle{initial:}
location:S${i}:start{committed:}
location:S${i}:processing{}
location:S${i}:done{committed:}

edge:S${i}:idle:start:finished$((i-1)){}
edge:S${i}:start:processing:start1{}
edge:S${i}:processing:done:start$((NSUBSTATIONS+1)){}
edge:S${i}:done:idle:finished${i}{}
"
done

#
for i in `seq 1 $NSTATIONS`; do
    for j in `seq 1 $NSUBSTATIONS`; do
	echo "# Substation $i $j
process:Sub${i}_${j}
clock:1:x${i}_${j}
location:Sub${i}_${j}:idle{initial:}
location:Sub${i}_${j}:processing{invariant: x${i}_${j}<=$PROCTIME}
edge:Sub${i}_${j}:idle:processing:start${j}{do: x${i}_${j}=0}
edge:Sub${i}_${j}:processing:idle:start$((j+1)){provided: x${i}_${j}<=$PROCTIME}
"
    done
done

#
echo "# Property
process:P
clock:1:z
location:P:idle{initial:}
location:P:error{labels: error}
location:P:finished
location:P:ok
edge:P:idle:finished:finished0{do: z=0}
edge:P:finished:error:tau{provided: z>$TIMEOUT}
edge:P:finished:ok:finished${NSTATIONS}{provided: z<=$TIMEOUT}
edge:P:ok:idle:tau{}
"

# Synchronizations

echo "# Synchronizations
sync:P@finished0:S1@finished0
sync:S${NSTATIONS}@finished${NSTATIONS}:P@finished${NSTATIONS}"

for i in `seq 1 $((NSTATIONS-1))`; do # between stations
    echo "sync:S${i}@finished${i}:S$((i+1))@finished${i}"
done

for i in `seq 1 $NSTATIONS`; do # between stations and substations
    echo "sync:S${i}@start1:Sub${i}_1@start1"
    echo "sync:S${i}@start$((NSUBSTATIONS+1)):Sub${i}_${NSUBSTATIONS}@start$((NSUBSTATIONS+1))"
done

for i in `seq 1 $NSTATIONS`; do # between substations
    for j in `seq 1 $((NSUBSTATIONS-1))`; do
	echo "sync:Sub${i}_${j}@start$((j+1)):Sub${i}_$((j+1))@start$((j+1))"
    done
done
echo ""
