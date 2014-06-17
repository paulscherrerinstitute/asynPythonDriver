#!../../bin/darwin-x86/python

## You may have to change python to something else
## everywhere it appears in this file

< envPaths
epicsEnvSet PYTHONPATH,${TOP}/scripts

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/scope.dbd"
scope_registerRecordDeviceDriver pdbbase

asynPythonDriverConfigure("PY1", "scope", 17)

## Load record instances
dbLoadRecords("${TOP}/db/scope.template", "P=MTEST:,R=SCOPE:,PORT=PY1,ADDR=0,NPOINTS=1000,TIMEOUT=1")

cd ${TOP}/iocBoot/${IOC}
iocInit

## Start any sequence programs
