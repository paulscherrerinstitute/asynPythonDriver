__all__ = ['PythonDriver']
import os
import param # param is an extension in asynPythonDriver library.

from DBParser import parse_database, find_params

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


DataType = {
    'Int32'        : param.Int32,
    'Float64'      : param.Float64,
    'Octet'        : param.Octet,
    'Int8Array'    : param.Int8Array,
    'Int16Array'   : param.Int16Array,
    'Int32Array'   : param.Int32Array,
    'Float32Array' : param.Float32Array,
    'Float64Array' : param.Float64Array,
}


class PythonDriverMeta(type):
    def __new__(self, clsname, base, clsdict):
        # Create the class definition
        cls = type.__new__(self, clsname, base, clsdict)
        # if database file is defined, load params from it
        if hasattr(cls, '__db__'):
            dbfile = getattr(cls, '__db__')
            # compose a list of path to search EPICS templates
            paths = []
            if 'TOP' in os.environ:
                paths.append(os.path.join(os.environ.get('TOP'), 'db'))
            paths.append(os.getcwd())
            for path in paths:
                dbfullpath = os.path.join(path, dbfile)
                if os.path.exists(dbfullpath):
                    records = parse_database(open(dbfullpath).read())
                    params = find_params(records)
                    for drvinfo, info in params.items():
                        param =  Param(drvinfo, DataType[info[1]])
                        setattr(cls, info[0], param)

        # Find Param instances and assocate the variable name with its Param name
        # It serves as a lookup table
        params = {}
        for name, value in vars(cls).items():
            if isinstance(value, Param):
                params[value.name] = name
        setattr(cls, 'params', params)

        return cls


class PythonDriver(object):
    __metaclass__ = PythonDriverMeta

    def read(self, reason):
        return getattr(self, self.params[reason])

    def readEnum(self, reason):
        pass

    def write(self, reason, value):
        setattr(self, self.params[reason], value)

    def update(self):
        param.callback(param.this) 

    def updateEnum(self, reason, enums):
        param.callbackEnum(param.this, reason, enums)

