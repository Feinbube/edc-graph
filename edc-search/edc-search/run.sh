#!/bin/bash

#RES=`OMP_PLACES='{90:1}' ./edc-search-adjacent-list -s -r 2 < /home/Felix.Eberhardt/Goessel/edc-search/graphs/cai-haratsch-mutlu-mai-2012-6.graph`
#OMP_PLACES='{90:15}' ./edc-search-adjacent-list -s -r 2 -f /home/Felix.Eberhardt/Goessel/edc-search/graphs/8.graph
OMP_PLACES='{'$1':15}:'$2':15' ./edc-search-adjacent-list -s -r 2 -f /home/Felix.Eberhardt/Goessel/edc-search/graphs/8.graph -t $3
#printf "%d\t%d\t%d\n" $1 $2 $RES >> results
