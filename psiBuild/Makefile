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

# Get the version string of python
PYTHON_VERSION = $(shell python -c 'from distutils import sysconfig; \
    print(sysconfig.get_config_var("VERSION"))')

USR_INCLUDES = $(shell python-config --includes)
USR_LDFALGS =  $(shell python-config --libs)
USR_SYS_LIBS += python$(PYTHON_VERSION)

