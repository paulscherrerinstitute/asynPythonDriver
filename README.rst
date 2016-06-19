asynPythonDriver
================

This driver derives from `asynPortDriver <http://www.aps.anl.gov/epics/modules/soft/asyn/R4-22/asynPortDriver.html>`_.
By its essence, the parameter value access is delegated to the Python module, provided by user.

Configure
---------
IOC shell command to configure a python port::

    /*
     * portName: The name of the asyn port driver to be created.
     * moduleName: The python module to import.
     * numParams: The number of parameters in this driver.
     */
    asynPythonDriverConfigure(const char *portName, const char *pythonModule, int numParams)

PythonDriver
------------
``PythonDriver`` is a base class that implements the connection between EPICS database and asynPortDriver parameter table. There are two steps for the derived class to define this connections.

Suppose we have an EPICS template file *test.template*,::

    record(ai, "RAND")
    {
        field (DTYP, "asynFloat64")
        field (INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))RAND")
        field (PREC, "4")
        field (SCAN, "I/O Intr")
        info  (pyname, "rand")
    }

The key is the *info (pyname, "rand")* field, that defines the variable name which we will refer to in our Python class.

In the Python module *test.py*, class ``TestPythonDriver``, ::

    import time
    import random
    import threading
    from PythonDriver import PythonDriver

    class TestPythonDriver(PythonDriver):
        __db__ = "test.template"
        def __init__(self):
            self.tid = threading.Thread(target=self.simTask)
            self.tid.start()

        def simTask(self):
            while True:
                self.rand = random.random()
                self.update()
                time.sleep(1)

    # create an instance
    driver = TestPythonDriver()

The *__db__* class attribute links Python class to EPICS database. The base class in turn creates class attributes for each records having *info (pyname, "")* specified. In this example, *rand* will be accessible just like a normal numeric value. Call base class method ``update`` to update the value.

In the startup script, ::

    ## add python module paths
    epicsEnvSet PYTHONPATH,${TOP}/python:${ASYNPYTHON}/python

    ...

    asynPythonDriverConfigure("PY2", "test", 1)

    ## Load record instances
    dbLoadRecords("${TOP}/db/test.template", "PORT=PY2,ADDR=0,TIMEOUT=1")


Examples
--------

Simulated Oscilloscope
^^^^^^^^^^^^^^^^^^^^^^

The driver from *testAsynPortDriver* has been ported. The EPICS database is copied, with addition of *info(pyname,"")* fields.
Only that the C++ file *testAsynPortDriver.cpp* has been written in Python *scope.py*.

::

    cd iocs
    make
    cd scopeIOC/iocBoot/iocscope
    ./start_epics

Launch MEDM::
    
    medm -x -macro P=MTEST:,R=SCOPE: testAsynPortDriver.adl

Known Problems
--------------

- For input records, ai, longin, stringin, the SCAN field can only be *I/O Intr*. Which means the records has to be updated by the driver and then call ``self.update``.

- Each asynPythonDriverConfigure creates a Python sub-interpreter, in order to isolate the modules. And watch out the `bug and caveats <https://docs.python.org/2/c-api/init.html#bugs-and-caveats>`_ about sub-interpreter.
