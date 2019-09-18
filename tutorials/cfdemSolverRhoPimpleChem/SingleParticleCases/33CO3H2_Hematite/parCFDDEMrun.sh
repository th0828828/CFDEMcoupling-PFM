#!/bin/bash

#===================================================================#
# allrun script for testcase as part of test routine 
# run settlingTest CFD part
# Christoph Goniva - Feb. 2011
#===================================================================#

#- source CFDEM env vars
. ~/.bashrc

#- include functions
source $CFDEM_PROJECT_DIR/etc/functions.sh

#--------------------------------------------------------------------------------#
#- define variables
casePath="$(dirname "$(readlink -f ${BASH_SOURCE[0]})")"
logpath=$casePath
headerText="GOD0k1H23pcnt"
logfileName="log_$headerText"
solverName="cfdemSolverRhoPimpleChem"
nrProcs="2"
machineFileName="none"   # yourMachinefileName | none
debugMode="off"          # on | off| strict
testHarnessPath="$CFDEM_TEST_HARNESS_PATH"
runOctave="false"
postproc="false"

#--------------------------------------------------------------------------------#

#- call function to run a parallel CFD-DEM case
parCFDDEMrun $logpath $logfileName $casePath $headerText $solverName $nrProcs $machineFileName $debugMode


if [ $runOctave == "true" ]
    then
        #------------------------------#
        #  octave

        #- change path
        cd octave

        #- rmove old graph
        rm cfdemSolverPiso_ErgunTestMPI.eps

        #- run octave
        octave totalPressureDrop.m

        #- show plot 
        evince cfdemSolverPiso_ErgunTestMPI.eps

        #- copy log file to test harness
        cp ../../$logfileName $testHarnessPath
        cp cfdemSolverPiso_ErgunTestMPI.eps $testHarnessPath
fi
    
if [ $postproc == "true" ]
  then

    #- keep terminal open (if started in new terminal)
    echo "simulation finished? ...press enter to proceed"
    read

    #- get VTK data from liggghts dump file
    cd $casePath/DEM/post
    python -i $CFDEM_LPP_DIR/lpp.py dump*.liggghts_run

    #- get VTK data from CFD sim
    cd $casePath/CFD
    reconstructPar
    foamToVTK                                                   #- serial run of foamToVTK
    #source $CFDEM_SRC_DIR/lagrangian/cfdemParticle/etc/functions.sh                       #- include functions
    #pseudoParallelRun "foamToVTK" $nrPostProcProcessors          #- pseudo parallel run of foamToVTK

    #- start paraview
    paraview
 
 #- keep terminal open (if started in new terminal)
    echo "...press enter to clean up case"
    echo "press Ctr+C to keep data"
    read

fi

#- clean up case
#echo "deleting data at: $casePath :\n"
#source $WM_PROJECT_DIR/bin/tools/CleanFunctions
#cd $casePath/CFD
#cleanCase
#rm -r $casePath/CFD/clockData
#rm $casePath/DEM/post/*.*
#touch $casePath/DEM/post/.gitignore
#rm $casePath/DEM/post/restart/*.*
#rm $casePath/DEM/post/restart/liggghts.restartCFDEM*
#touch $casePath/DEM/post/restart/.gitignore
#echo "done"
