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

# Get the version string of python
PYTHON_VERSION = $(shell python -c 'from distutils import sysconfig; \
    print(sysconfig.get_config_var("VERSION"))')

USR_INCLUDES = $(shell python-config --includes)
USR_LDFALGS =  $(shell python-config --libs)
USR_SYS_LIBS += python$(PYTHON_VERSION)

asynPythonDriver_LIBS += asyn
asynPythonDriver_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

