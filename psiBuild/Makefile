include /ioc/tools/driver.makefile
PROJECT=asynPythonDriver
BUILDCLASSES=Linux
EXCLUDE_VERSIONS=3.14.8
CROSS_COMPILER_TARGET_ARCHS=$(EPICS_HOST_ARCH)

DBDS += ../pythonApp/src/asynPythonDriver.dbd
SRCS += ../pythonApp/src/asynPythonDriver.cpp

SCRIPTS += startup.cmd
SCRIPTS += ../pythonApp/python/DBParser.py
SCRIPTS += ../pythonApp/python/PythonDriver.py

USR_INCLUDES = $(shell python3-config --includes)
USR_LDFLAGS =  $(shell python3-config --libs)
