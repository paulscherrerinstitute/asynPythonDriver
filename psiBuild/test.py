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

driver = TestPythonDriver()
