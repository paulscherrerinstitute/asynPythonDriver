TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += asynPythonDriver

DBD += asynPythonDriver.dbd

# specify all source files to be compiled and added to the library
asynPythonDriver_SRCS += asynPythonDriver.cpp

USR_INCLUDES := $(shell python3-config --includes)
USR_LDFLAGS :=  $(shell python3-config --ldflags --embed)
ifeq ($(firstword $(USR_LDFLAGS)),Usage:)
  USR_LDFLAGS :=  $(shell python3-config --ldflags)
endif

asynPythonDriver_LIBS += asyn
asynPythonDriver_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

