import numpy
import time
import threading

from PythonDriver import PythonDriver

FREQUENCY=1000 #      /* Frequency in Hz */
AMPLITUDE=1.0  #      /* Plus and minus peaks of sin wave */
NUM_DIVISIONS=10 #    /* Number of scope divisions in X and Y */

class ScopeDriver(PythonDriver):
    # these parameters must match the definitions of EPICS database
    # The first argument is the drvInfo string and the second is the data type
    # Ideally this could be parsed from EPICS database.
    # The variable name could be inferred from record name or the info fields, e.g. info(pyname, "x")
    __db__ = 'scope.template'

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
