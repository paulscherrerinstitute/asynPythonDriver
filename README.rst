asynPythonDriver
================

This driver derives from `asynPortDriver <http://www.aps.anl.gov/epics/modules/soft/asyn/R4-22/asynPortDriver.html>`_.
By its essence, the parameter value access is delegated to the Python module, provided by user.

Configure
---------

::

    /*
     * portName: The name of the asyn port driver to be created.
     * moduleName: The python module to import.
     * numParams: The number of parameters in this driver.
     */
    asynPythonConfigure(const char *portName, const char *pythonModule, int numParams)

Examplies
---------

Simulated Oscilloscope
^^^^^^^^^^^^^^^^^^^^^^

The driver is from *testAsynPortDriver* has been ported. The EPICS database is copied with no change. 
Only that the C++ file testAsynPortDriver.cpp has been written in Python scope.py. 


