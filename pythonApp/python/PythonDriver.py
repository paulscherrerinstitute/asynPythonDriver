__all__ = ['PythonDriver']
import os
import param # param is an extension in asynPythonDriver library.

import pyparsing as pp
def parseDb(content):
    """
    Parse an EPICS.db file.

    Return a list of EPICS record dictionaries. Each record dict contains the EPICS-field
    and value pairs and additional keys for:

    - recordname
    - alias
    - RTYP
    - info {INFOKEY:INFOVALUE,}
    """
    recordName= pp.ZeroOrMore(pp.Suppress(pp.Word('"'))) +\
                pp.Word(pp.alphanums+"_-+:[]<>;$()") +\
                pp.ZeroOrMore(pp.Suppress(pp.Word('"')))
    qString   = pp.dblQuotedString.setParseAction(pp.removeQuotes) # double quoted String, quotes removed

    comment   = pp.Suppress(pp.pythonStyleComment)
    alias     = pp.Group(pp.Keyword("alias") +\
                pp.Suppress(pp.Word("(")) +\
                recordName +  pp.Suppress(pp.Word(",")) +\
                recordName + pp.Suppress(pp.Word(")")) )
    field     = pp.Group((pp.Keyword("field")^pp.Keyword("info")) +\
                pp.Suppress(pp.Word("(")) + pp.Word(pp.alphanums) + pp.Suppress(pp.Word(",")) + qString + pp.Suppress(pp.Word(")")) +\
                pp.ZeroOrMore(comment) ^\
                pp.Keyword("alias") + pp.Suppress(pp.Word("(")) + recordName +  pp.Suppress(pp.Word(")")) + pp.ZeroOrMore(comment))
    record    = pp.Group(pp.Keyword("record") + pp.Suppress(pp.Word("(")) +\
                    pp.Word(pp.alphanums) + pp.Suppress(pp.Word(",")) + recordName +\
                pp.Suppress(pp.Word(")")) + pp.ZeroOrMore(comment) +\
                pp.Suppress(pp.Word("{")) + pp.ZeroOrMore(comment) + pp.Group(pp.ZeroOrMore(field)) + pp.Suppress(pp.Word("}")))
    epicsDb   = pp.OneOrMore(comment ^ alias ^ record)

    recordList = []
    for pGroup in epicsDb.parseString(content):
        if pGroup[0] == 'record':
            rec = {'RTYP':pGroup[1],'recordname':pGroup[2]}
            if len(pGroup) <= 3:
                    continue
            for fields in pGroup[3]:
                if fields[0] == 'field':     # ['field','fieldType','fieldValue']
                    rec[fields[1]]=fields[2]
                if fields[0] == 'alias':
                    rec['alias'] = fields[1]
                if fields[0] == 'info':
                    if not rec.has_key('info'):
                        rec['info'] = {}
                    rec['info'][fields[1]] = fields[2]
            recordList.append(rec)
    return recordList

import re
def findParams(records):
    # regexp patterns to remove macros like $(P)
    macro_pat = re.compile('\$\(.*?\)')
    # regexp pattern to extract drv info string 
    link_pat = re.compile('^@asyn\(.*\)\s*(.*)')
    # extract data type info from device type (DTYP)
    dtype_pat = re.compile('^asyn(.*?)(|In|Out|Read|Write)$')

    params = {}
    for record in records:
        recname = macro_pat.sub('', record['recordname'])
        # strip off _RBV suffix, this is only a convention
        recname = recname.rstrip('_RBV')

        # check dtype is asynXXX(In|Out|Read|Write)
        m = dtype_pat.match(record.get('DTYP', ''))
        if m is None or m.group(1) == '':
            continue
        dtype = m.group(1)

        # get INP or OUT link
        if 'INP' in record:
            iolink = record['INP']
        elif 'OUT' in record:
            iolink = record['OUT']
        else:
            continue
        # check link is of form @asyn()drvinfo
        m = link_pat.match(iolink)
        if m is None or m.group(1) == '':
            continue
        drvinfo = m.group(1)

        # try 'pyname' info
        pyname = ''
        if 'info' in record:
            if 'pyname' in record['info']:
                pyname = record['info']['pyname']

        if drvinfo not in params:
            if pyname != '':
                name = pyname
            else:
                name = recname
            params[drvinfo] = (name, dtype)
    return params


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
                    records = parseDb(open(dbfullpath).read())
                    params = findParams(records)
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

