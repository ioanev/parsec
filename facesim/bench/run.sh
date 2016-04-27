#!/bin/bash

VERSION=$1
INPUT=$2
NTHREADS=$3
NDIVS=$4

if [ -z "$NDIVS" ]; then
    NDIVS=${NTHREADS}
fi

#DEFINE ARGS
case $INPUT in
  "native") ARGS="-timing -lastframe 100 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
  "simlarge") ARGS="-timing -lastframe 1 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
  "simmedium") ARGS="-timing -lastframe 1 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
  "simsmall") ARGS="-timing -lastframe 1 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
  "simdev") ARGS="-timing -lastframe 1 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
  "test") ARGS="-timing -lastframe 1 -inputdir ${ROOT}/facesim/inputs -outputdir Storytelling/output";;
esac

#ADD THREADS/PARTITIONING SCHEME
case ${VERSION} in
    ompss*)
        NX_ARGS="--threads=${NTHREADS} ${NX_ARGS}"
        ARGS+=" -threads ${NDIVS}"
        ;;
    omp* )
	    export OMP_NUM_THREADS=${NTHREADS}
        ARGS+=" -threads ${NDIVS}"
        ;;
    pthreads* | serial*)
        ARGS+=" -threads ${NTHREADS}"
        ;;
    *)
        echo -e "\033[0;31mVERSION = $VERSION not correct, stopping $BENCHID run\033[0m"
        exit
        ;;
esac


#RUN PROGRAM
${ROOT}/facesim/bin/facesim-${VERSION} ${ARGS}

