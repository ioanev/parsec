#!/bin/bash

VERSION=$1
INPUT=$2
NTHREADS=$3
EXTRA_ARGS=$4

BENCHPATH=${ROOT}/canneal

case $INPUT in
  "native") ARGS="15000 2000 ${BENCHPATH}/inputs/2500000.nets 6000";;
  "simlarge") ARGS="15000 2000 ${BENCHPATH}/inputs/400000.nets 128";;
  "simmedium") ARGS="15000 2000 ${BENCHPATH}/inputs/200000.nets 64";;
  "simsmall") ARGS="10000 2000 ${BENCHPATH}/inputs/100000.nets 32";;
  "simdev") ARGS="100 300 ${BENCHPATH}/inputs/100.nets 2";;
  "test") ARGS="5 100 ${BENCHPATH}/inputs/10.nets 1";;
esac

if [ $VERSION = "omp" ]
then
	export OMP_NUM_THREADS=${NTHREADS}
fi

NX_ARGS="${EXTRA_ARGS} --threads=${NTHREADS}" ${BENCHPATH}/bin/canneal-${VERSION} ${NTHREADS} ${ARGS}
