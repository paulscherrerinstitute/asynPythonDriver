TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC application

PROD_IOC = scope
# scope.dbd will be created and installed
DBD += scope.dbd

# scope.dbd will be made up from these files:
scope_DBD += base.dbd
scope_DBD += asyn.dbd
scope_DBD += asynPythonDriver.dbd

# scope_registerRecordDeviceDriver.cpp derives from scope.dbd
scope_SRCS += scope_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
scope_SRCS_DEFAULT += scopeMain.cpp
scope_SRCS_vxWorks += -nil-

# Link in the code from the support library
scope_LIBS += asyn asynPythonDriver

# Finally link to the EPICS Base libraries
scope_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

