
import param # param is an extension in asynPythonDriver library.

class Param(object):
    """
    A descriptor class to ease parameter access.

    .. note:: The cache is done in instance level.
    """
    def __init__(self, name, dtype):
        self.name = name
        self.dtype= dtype
        self.value = None
        # when param got created in python, create it also in C++
        self.reason = param.create(param.this, name, dtype)

    def __get__(self, obj, obj_type):
        # return self if called from class object
        if obj is None:
            return self 
        # when param got read, read the value from C++ 
        if self.value is None:
            return param.get(param.this, self.name, self.dtype)
        else:
            return self.value

    def __set__(self, obj, value):
        # cache the value
        self.value = value
        # when param got written, write the value to C++
        param.set(param.this, self.name, self.dtype, value)


import numpy
import time
import threading


FREQUENCY=1000 #      /* Frequency in Hz */
AMPLITUDE=1.0  #      /* Plus and minus peaks of sin wave */
NUM_DIVISIONS=10 #    /* Number of scope divisions in X and Y */


class PythonDriverMeta(type):
    def __new__(self, clsname, base, clsdict):
        # Create the class definition
        cls = type.__new__(self, clsname, base, clsdict)
        # Find Param instances and assocate the variable name with its Param name
        # It serves as a lookup table
        params = {}
        for name, value in clsdict.items():
            if isinstance(value, Param):
                params[value.name] = name
        setattr(cls, 'params', params)

        return cls


class PythonDriver(object):
    __metaclass__ = PythonDriverMeta

    def update(self):
        param.callback(param.this) 

    def updateEnum(self, reason, enums):
        param.callbackEnum(param.this, reason, enums)


class ScopeDriver(PythonDriver):
    # these parameters must match the definitions of EPICS database
    # The first argument is the drvInfo string and the second is the data type
    # Ideally this could be parsed from EPICS database.
    # The variable name could be inferred from record name or the info fields, e.g. info(pyname, "x")
    run = Param("SCOPE_RUN", param.Int32)
    npoints = Param("SCOPE_MAX_POINTS", param.Int32)
    refresh = Param("SCOPE_UPDATE_TIME", param.Float64)

    mini = Param("SCOPE_MIN_VALUE",  param.Float64)
    maxi = Param("SCOPE_MAX_VALUE",  param.Float64)
    mean = Param("SCOPE_MEAN_VALUE",  param.Float64)

    waveform = Param("SCOPE_WAVEFORM",  param.Float64Array)
    timebase = Param("SCOPE_TIME_BASE",  param.Float64Array)

    noise = Param("SCOPE_NOISE_AMPLITUDE", param.Float64)
    delay = Param("SCOPE_TRIGGER_DELAY", param.Float64)
    
    offset = Param("SCOPE_VOLT_OFFSET", param.Float64)
    gain_menu = Param("SCOPE_VERT_GAIN_SELECT", param.Int32)
    gain = Param("SCOPE_VERT_GAIN", param.Float64)
    voltsdiv_menu = Param("SCOPE_VOLTS_PER_DIV_SELECT", param.Int32)
    voltsdiv = Param("SCOPE_VOLTS_PER_DIV", param.Float64)
    timediv_menu = Param("SCOPE_TIME_PER_DIV_SELECT", param.Int32)
    timediv = Param("SCOPE_TIME_PER_DIV", param.Float64)

    def __init__(self):
        # Initialize parameters
        self.npoints = 1000
        self.gain_menu = 10
        self.setGain()
        self.voltsdiv = 1.0
        self.timediv = 0.001
        self.refresh = 0.5
        self.noise = 0.1
        self.mini = 0
        self.maxi = 0
        self.mean = 0
        self.delay = 0
        self.offset = 0
        self.run = 0
        self.timebase = numpy.linspace(0, 1, self.npoints) * NUM_DIVISIONS

        self.event = threading.Event()
        self.tid = threading.Thread(target=self.simTask)
        self.tid.start()


    def setGain(self):
        igain = self.gain_menu
        self.gain = igain
        self.voltsdiv_enum = []
        for i in [1., 2., 5., 10.]:
            value = int(i/igain * 1000 + 0.5)
            self.voltsdiv_enum += [('%.2f'%(i/igain), value, 0)]
        self.updateEnum(ScopeDriver.voltsdiv_menu.name, self.voltsdiv_enum)

    def read(self, reason):
        return getattr(self, self.params[reason])

    def readEnum(self, reason):
        if reason == ScopeDriver.voltsdiv_menu.name:
            return self.voltsdiv_enum

    def write(self, reason, value):
        # store the value
        setattr(self, self.params[reason], value)
        
        if reason == ScopeDriver.run.name:
            if value == 1:
                self.event.set()
        elif reason == ScopeDriver.gain_menu.name:
            self.setGain()
        elif reason == ScopeDriver.timediv_menu.name:
            self.timediv = value / 1000000.
        elif reason == ScopeDriver.voltsdiv_menu.name:
            self.voltsdiv = value / 1000.

    def simTask(self):
        while (True):
            if self.run != 1:
                self.event.wait()
                self.event.clear()
            # create time series
            timebase = self.delay + numpy.linspace(0, self.timediv * NUM_DIVISIONS, self.npoints)
            # create waveform
            waveform = AMPLITUDE * numpy.sin(timebase * FREQUENCY*2*numpy.pi) + \
                    self.noise * numpy.random.random((self.npoints,))
            # apply offset and scale
            self.waveform = NUM_DIVISIONS/2 + 1. / self.voltsdiv * (self.offset + waveform)
            # statistics calculated without offset and scale
            self.mini = waveform.min()
            self.maxi = waveform.max()
            self.mean = waveform.mean()

            self.update()
            time.sleep(self.refresh)


driver = ScopeDriver()
