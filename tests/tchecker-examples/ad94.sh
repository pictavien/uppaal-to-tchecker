#!/bin/sh

# This file is a part of the TChecker project.
#
# See files AUTHORS and LICENSE for copyright details.

#clock:size:name
#int:size:min:max:init:name
#process:name
#event:name
#location:process:name{attributes}
#edge:process:source:target:event:{attributes}
#sync:events
#   where
#   attributes is a colon-separated list of key:value
#   events is a colon-separated list of process@event
cat << EOF
system:ad94_fig10

clock:1:x
clock:1:y

event:a
event:b
event:c
event:d

process:P
location:P:l0{initial:}
location:P:l1{}
location:P:l2{}
location:P:l3{labels: green}
edge:P:l0:l1:a{do:y=0}
edge:P:l1:l2:b{provided: y==1}
edge:P:l1:l3:c{provided: x<1}
edge:P:l2:l3:c{provided: x<1}
edge:P:l3:l1:a{provided: y<1 : do:y=0}
edge:P:l3:l3:d{provided: x>1}
EOF
