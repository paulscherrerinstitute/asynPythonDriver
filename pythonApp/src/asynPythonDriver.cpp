/**
 * asynPythonDriver.cpp
 *
 * Disptach asyn port access to Python modules.
 *
 * Author: Xiaoqiang Wang
 *
 * Created June 13, 2014
 */
#include <Python.h>

#if PY_MAJOR_VERSION >= 3
    #define PyString_FromString PyUnicode_FromString
    #define PyString_AsString PyUnicode_AsUTF8
    #define CAPSULE_BUILD(ptr,name, destr) PyCapsule_New(ptr, name, destr)
    #define CAPSULE_CHECK(obj) PyCapsule_CheckExact(obj)
    #define CAPSULE_EXTRACT(obj,name) PyCapsule_GetPointer(obj, name)
#else
    #define CAPSULE_BUILD(ptr,name, destr) PyCObject_FromVoidPtr(ptr, destr)
    #define CAPSULE_CHECK(obj) PyCObject_Check(obj)
    #define CAPSULE_EXTRACT(obj,name) PyCObject_AsVoidPtr(obj)
#endif

#include <string>

#include <epicsString.h>
#include <iocsh.h>

#include <epicsExport.h>
#include "asynPythonDriver.h"

static const char *driverName = "asynPythonDriver";

/* Helper function to convert from Python to C */

/* Convert array valuse from Python to C.
 * \param[in] pValue The Python object has PyBuffer or PySequence interface
 * \param[in] value  The pointer to the memory to be written. It can be NULL, in which case a new memory will be allocated and returned.
 * \param[in] nElements Number of elements in the array if it is NULL.
 * \param[out] nIn Number of elements in the output array.
 *
 */
template <typename epicsType> 
static epicsType* getArrayFromPython(PyObject *pValue, epicsType *value, size_t nElements, size_t *nIn)
{
    if (PyObject_CheckBuffer(pValue)) {
        size_t length;
        Py_buffer buffer;
        if(PyObject_GetBuffer(pValue, &buffer, PyBUF_SIMPLE) != 0) {
            PyErr_Print();
        } else {
            length = buffer.len / sizeof(epicsType);
            /* alloc the buffer */
            if (value == NULL) {
                value = (epicsType *)malloc(length * sizeof(epicsType));
            } else {
                if (length > nElements) length = nElements;
            }
            memcpy((void*)value, buffer.buf, length * sizeof(epicsType));
            *nIn = length;
        }
    } else if (PySequence_Check(pValue)) {
        size_t length = PySequence_Length(pValue);
        if (value == NULL) {
            value = (epicsType *)malloc(length * sizeof(epicsType));
        } else {
            if (length > nElements) length = nElements;
        }
        PyObject *item;
        for (size_t i=0; i<length; i++) {
            item = PySequence_GetItem(pValue, i);
            value[i] = (epicsType)PyFloat_AsDouble(item);
            Py_DECREF(item);
        }
        *nIn = length;
    }   

    return value;
}

/* Helper function to convert enum from Python to C
 *
 */
static asynStatus getEnumFromPython(PyObject *pValue, char *strings[], int values[], int severities[], size_t nElements, size_t *nIn)
{
    *nIn = 0;
    asynStatus status = asynError;

    if (!PySequence_Check(pValue))
        return status;

    int length = PySequence_Length(pValue);
    int i = 0;
    for (; i<length; i++) {
        char *name;
        int value, severity;
        PyObject *pTuple = PySequence_GetItem(pValue, i);
        if (!PyArg_ParseTuple(pTuple, "sii", &name, &value, &severity)) {
            Py_DECREF(pTuple); 
            PyErr_Print();
            break;
        }
        Py_DECREF(pTuple); 
        if (strings[i]) free(strings[i]);
        strings[i] = epicsStrDup(name);
        values[i] = value;
        severities[i] = severity;
    }
    if (i > 0) {
        *nIn = i;
        status = asynSuccess;
    }

    return status;
}


/* Python extension */

static PyObject *Py_createParam(PyObject *self, PyObject*args)
{
    asynParamType type;
    int reason;
    char *name;
    char error[256];
    asynStatus status;
    PyObject *pThis;
    PyObject *pRes;

    if(!PyArg_ParseTuple(args, "Osi", &pThis, &name, &type))
        return NULL;

    if (!CAPSULE_CHECK(pThis))
        return NULL;

    asynPythonDriver *pDriver = reinterpret_cast<asynPythonDriver *>(CAPSULE_EXTRACT(pThis, "asynPythonDriver"));
    if (pDriver == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid asynPythonDriver instance");
        return NULL;
    }

    status = pDriver->createParam(name, type, &reason);
    if (status != asynSuccess) {
        sprintf(error, "Failed to create parameter %s", name);
        PyErr_SetString(PyExc_AttributeError, error);
        return NULL;
    }

    pRes = PyLong_FromLong(reason);
    return pRes;
}

static PyObject *Py_getParam(PyObject *self, PyObject *args)
{
    asynParamType type;
    int reason;
    char *name;
    int ivalue;
    double dvalue;
    char svalue[128];
    char error[256];
    asynStatus status;
    PyObject *pThis;
    PyObject *pRes;

    if(!PyArg_ParseTuple(args, "Osi", &pThis, &name, &type))
        return NULL;

    if (!CAPSULE_CHECK(pThis))
        return NULL;

    asynPythonDriver *pDriver = reinterpret_cast<asynPythonDriver *>(CAPSULE_EXTRACT(pThis, "asynPythonDriver"));
    if (pDriver == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid asynPythonDriver instance");
        return NULL;
    }

    status = pDriver->findParam(name, &reason);
    if (status != asynSuccess) {
        sprintf(error, "Parameter %s not found", name);
        PyErr_SetString(PyExc_AttributeError, error);
        return NULL;
    }

    switch(type) {
        case asynParamInt32:
        case asynParamUInt32Digital:
            pDriver->getIntegerParam(reason, &ivalue);
            pRes = PyLong_FromLong(ivalue);
            break;
        case asynParamFloat64:
            pDriver->getDoubleParam(reason, &dvalue);
            pRes = PyFloat_FromDouble(dvalue);
            break;
        case asynParamOctet:
            pDriver->getStringParam(reason, 128, svalue);
            pRes = PyString_FromString(svalue);
            break;
        default:
            sprintf(error, "Invalid parameter type %d", type);
            PyErr_SetString(PyExc_TypeError, error);
            return NULL;
    }
    return pRes;
}

static PyObject *Py_setParam(PyObject *self, PyObject *args)
{
    asynParamType type;
    int reason;
    char *name;
    int ivalue;
    double dvalue;
    char *svalue;
    char error[256];
    asynStatus status;
    PyObject *pThis;
    PyObject *pValue;

    if(!PyArg_ParseTuple(args, "OsiO", &pThis, &name, &type, &pValue))
        return NULL;

    if (!CAPSULE_CHECK(pThis))
        return NULL;

    asynPythonDriver *pDriver = reinterpret_cast<asynPythonDriver *>(CAPSULE_EXTRACT(pThis, "asynPythonDriver"));
    if (pDriver == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid asynPythonDriver instance");
        return NULL;
    }

    status = pDriver->findParam(name, &reason);
    if (status != asynSuccess) {
        sprintf(error, "Parameter %s not found", name);
        PyErr_SetString(PyExc_AttributeError, error);
        return NULL;
    }
    switch(type) {
        case asynParamInt32:
        case asynParamUInt32Digital:
            ivalue = PyLong_AsLong(pValue);
            pDriver->setIntegerParam(reason, ivalue);
            break;
        case asynParamFloat64:
            dvalue = PyFloat_AsDouble(pValue);
            pDriver->setDoubleParam(reason, dvalue);
            break;
        case asynParamOctet:
            svalue = PyString_AsString(pValue);
            pDriver->setStringParam(reason, svalue);
            break;
        case asynParamInt8Array:
            {
                size_t length;
                epicsInt8 *pArray = getArrayFromPython<epicsInt8>(pValue, NULL, 0, &length);
                if (pArray != NULL) {
                    pDriver->doCallbacksInt8Array(pArray, length, reason, 0);
                    free(pArray);
                }
            }
         case asynParamFloat64Array:
            {
                size_t length;
                epicsFloat64 *pArray = getArrayFromPython<epicsFloat64>(pValue, NULL, 0, &length);
                if (pArray != NULL) {
                    pDriver->doCallbacksFloat64Array(pArray, length, reason, 0);
                    free(pArray);
                }
            }
            break;
        default:
            sprintf(error, "Invalid parameter type %d", type);
            PyErr_SetString(PyExc_TypeError, error);
            return NULL;
    }
 
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Py_callbackParam(PyObject *self, PyObject *args)
{
    asynStatus status;
    PyObject *pThis;

    if(!PyArg_ParseTuple(args, "O", &pThis))
        return NULL;

    if (!CAPSULE_CHECK(pThis)) {
        return NULL;
    }
    asynPythonDriver *pDriver = reinterpret_cast<asynPythonDriver *>(CAPSULE_EXTRACT(pThis, "asynPythonDriver"));
    if (pDriver == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid asynPythonDriver instance");
        return NULL;
    }

    status = pDriver->callParamCallbacks();
    if (status != asynSuccess)
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Py_callbackEnum(PyObject *self, PyObject *args)
{
    asynStatus status;
    char *name;
    int reason;
    char error[128];
    PyObject *pThis, *pValue;

    if(!PyArg_ParseTuple(args, "OsO", &pThis, &name, &pValue))
        return NULL;

    if (!CAPSULE_CHECK(pThis)) {
        return NULL;
    }
    asynPythonDriver *pDriver = reinterpret_cast<asynPythonDriver *>(CAPSULE_EXTRACT(pThis, "asynPythonDriver"));
    if (pDriver == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid asynPythonDriver instance");
        return NULL;
    }

    status = pDriver->findParam(name, &reason);
    if (status != asynSuccess) {
        sprintf(error, "Parameter %s not found", name);
        PyErr_SetString(PyExc_AttributeError, error);
        return NULL;
    }
 
    char **strings = (char **)calloc(32, sizeof(char*));
    int *values = (int *)calloc(32, sizeof(int));
    int *severities = (int *)calloc(32, sizeof(int));
    size_t nElements;

    status = getEnumFromPython(pValue, strings, values, severities, 32, &nElements);
    if (status != asynSuccess) 
        return NULL;

    status = pDriver->doCallbacksEnum(strings, values, severities, nElements, reason, 0);
    if (status != asynSuccess)
        return NULL;

    free(strings);
    free(values);
    free(severities);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef ParamMethods[] = {
    {"create", Py_createParam, METH_VARARGS, "Create entry in paramList"},
    {"set", Py_setParam, METH_VARARGS, "Set value in paramList"},
    {"get", Py_getParam, METH_VARARGS, "Get value from paramList"},
    {"callback", Py_callbackParam, METH_VARARGS, "Signal changes from paramList"},
    {"callbackEnum", Py_callbackEnum, METH_VARARGS, "Signal changes from enum"},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef ParamModule = {
  PyModuleDef_HEAD_INIT,
  "param", /* name */
  "asynPortDriver param access", /* doc */
  -1, /* size */
  ParamMethods, /* methods */
  NULL, /* reload */
  NULL, /* traverse */
  NULL, /* clear */
  NULL, /* free */
};

#endif




/** Called when asyn clients call pasynEnum->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] strings Array of string pointers. 
  * \param[in] values Array of values 
  * \param[in] severities Array of severities 
  * \param[in] nElements Size of value array 
  * \param[out] nIn Number of elements actually returned */
asynStatus asynPythonDriver::readEnum(asynUser *pasynUser, char *strings[], int values[], int severities[], size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status;
    const char *paramName;
    const char *functionName = "readEnum";

    /* Default is not implemented */
    *nIn = 0;
    status = asynError;

    if (pFuncReadEnum == NULL) {
        return status;
    }

    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    /* Call readEnum method of Python module */
    PyEval_RestoreThread(this->pThreadState);
    PyObject *pArgs = Py_BuildValue("(s)", paramName);
    PyObject *pRes = PyObject_CallObject(pFuncReadEnum, pArgs);
    if (pRes == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: function=%d, name=%s read error\n", 
              driverName, functionName, function, paramName);
        PyErr_Print();
    } else {
        status = getEnumFromPython(pRes, strings, values, severities, nElements, nIn);
    }

    Py_XDECREF(pRes);
    Py_XDECREF(pArgs);
    PyEval_SaveThread();

    return status;
}

/** Called when asyn clients call pasynOctet->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus asynPythonDriver::writeOctet(asynUser *pasynUser, const char *value, size_t nChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName;
    const char *functionName = "writeOctet";

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(function, (char *)value);

    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);
    PyEval_RestoreThread(this->pThreadState);

    /* Call write method of Python module */
    PyObject *pArgs = Py_BuildValue("(ss)", paramName, value);
    PyObject *pRes = PyObject_CallObject(pFuncWrite, pArgs);
    if (pRes == NULL) {
        PyErr_Print();
        status = asynError;
    }
    Py_XDECREF(pRes);
    Py_XDECREF(pArgs);
    PyEval_SaveThread();

    /* Do callbacks so higher layers see any changes */
    status = (asynStatus) callParamCallbacks();

    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%s", 
                  driverName, functionName, status, function, paramName, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%s\n", 
              driverName, functionName, function, paramName, value);
    *nActual = nChars;
    return status;
}

/** Called when asyn clients call pasynInt32->read().
  * The base class implementation simply returns the value from the parameter library.  
  * Derived classes rarely need to reimplement this function.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[out] value Address of the value to read. */
asynStatus asynPythonDriver::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    int addr=0;
    asynStatus status = asynSuccess;
    epicsTimeStamp timeStamp; getTimeStamp(&timeStamp);
    static const char *functionName = "readInt32";
    
    status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
    /* We just read the current value of the parameter from the parameter library.
     * Those values are updated whenever anything could cause them to change */
    status = (asynStatus) getIntegerParam(addr, function, value);
    /* Set the timestamp */
    pasynUser->timestamp = timeStamp;
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, value=%d", 
                  driverName, functionName, status, function, *value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n", 
              driverName, functionName, function, *value);
    return(status);
}


/** Called when asyn clients call pasynInt32->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus asynPythonDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName;
    const char* functionName = "writeInt32";

    /* Set the parameter in the parameter library. */
    status = (asynStatus) setIntegerParam(function, value);
    
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    /* Call write method of Python module */
    PyEval_RestoreThread(this->pThreadState);
    PyObject *pArgs = Py_BuildValue("(si)", paramName, value);
    PyObject *pRes = PyObject_CallObject(pFuncWrite, pArgs);
    if (pRes == NULL) {
        PyErr_Print();
        status = asynError;
    }
    Py_XDECREF(pRes);
    Py_XDECREF(pArgs);
    PyEval_SaveThread();

    /* Do callbacks so higher layers see any changes */
    status = (asynStatus) callParamCallbacks();
    
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%d", 
                  driverName, functionName, status, function, paramName, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%d\n", 
              driverName, functionName, function, paramName, value);
    return status;
}

/** Called when asyn clients call pasynFloat64->read().
  * The base class implementation simply returns the value from the parameter library.  
  * Derived classes rarely need to reimplement this function.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the value to read. */
asynStatus asynPythonDriver::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int function = pasynUser->reason;
    int addr=0;
    asynStatus status = asynSuccess;
    epicsTimeStamp timeStamp; getTimeStamp(&timeStamp);
    static const char *functionName = "readFloat64";
    
    status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);
    /* We just read the current value of the parameter from the parameter library.
     * Those values are updated whenever anything could cause them to change */
    status = (asynStatus) getDoubleParam(addr, function, value);
    /* Set the timestamp */
    pasynUser->timestamp = timeStamp;
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, value=%f", 
                  driverName, functionName, status, function, *value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, *value);
    return(status);
}

/** Called when asyn clients call pasynFloat64->write().
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus asynPythonDriver::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName;
    static const char *functionName = "writeFloat64";

    /* Set the parameter and readback in the parameter library. */
    status = setDoubleParam(function, value);

    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    /* Call write method of Python module */
    PyEval_RestoreThread(this->pThreadState);
    PyObject *pArgs = Py_BuildValue("(sd)", paramName, value);
    PyObject *pRes = PyObject_CallObject(pFuncWrite, pArgs);
    if (pRes == NULL) {
        PyErr_Print();
        status = asynError;
    }
    Py_XDECREF(pRes);
    Py_XDECREF(pArgs);
    PyEval_SaveThread();

    /* Do callbacks so higher layers see any changes */
    status = (asynStatus) callParamCallbacks();
    
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%f", 
                  driverName, functionName, status, function, paramName, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%f\n", 
              driverName, functionName, function, paramName, value);
    return status;
}

template <typename epicsType> 
asynStatus asynPythonDriver::readArray(asynUser *pasynUser, epicsType *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status = asynError;
    const char *paramName;
    const char* functionName = "readArray";

    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    /* Call read method of Python module */
    PyEval_RestoreThread(this->pThreadState);
    PyObject *pArgs = Py_BuildValue("(s)", paramName);
    PyObject *pValue = PyObject_CallObject(pFuncRead, pArgs);
    if (pValue == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: function=%d, name=%s read error\n", 
              driverName, functionName, function, paramName);
        PyErr_Print();
    } 
    else {
        getArrayFromPython<epicsType>(pValue, value, nElements, nIn);
        status = asynSuccess;
    }

    Py_XDECREF(pValue);
    Py_XDECREF(pArgs);
    PyEval_SaveThread();

    return status;
}

/** Called when asyn clients call pasynInt8Array->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus asynPythonDriver::readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                size_t nElements, size_t *nIn)
{
    return readArray<epicsInt8>(pasynUser, value, nElements, nIn);
}

/** Called when asyn clients call pasynInt16Array->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus asynPythonDriver::readInt16Array(asynUser *pasynUser, epicsInt16 *value,
                                size_t nElements, size_t *nIn)
{
    return readArray<epicsInt16>(pasynUser, value, nElements, nIn);
}

/** Called when asyn clients call pasynInt32Array->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus asynPythonDriver::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                size_t nElements, size_t *nIn)
{
    return readArray<epicsInt32>(pasynUser, value, nElements, nIn);
}

/** Called when asyn clients call pasynFloat32Array->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus asynPythonDriver::readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                size_t nElements, size_t *nIn)
{
    return readArray<epicsFloat32>(pasynUser, value, nElements, nIn);
}

/** Called when asyn clients call pasynFloat64Array->read().
  * The base class implementation simply prints an error message.  
  * Derived classes may reimplement this function if required.
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Pointer to the array to read.
  * \param[in] nElements Number of elements to read.
  * \param[out] nIn Number of elements actually read. */
asynStatus asynPythonDriver::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                size_t nElements, size_t *nIn)
{
    return readArray<epicsFloat64>(pasynUser, value, nElements, nIn);
}


/** Report status of the driver.
  * This method calls the report function in the asynPortDriver base class.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >1 then print more.
  */
void asynPythonDriver::report(FILE *fp, int details)
{
    asynPortDriver::report(fp, details);
    if (details > 1) {
    }
}


/** This is the constructor for the asynPythonDriver class.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] moduleName The python module to import.
  * \param[in] numParams The number of parameters in this driver.
  */

asynPythonDriver::asynPythonDriver(const char *portName, const char *moduleName, int numParams)
    : asynPortDriver(portName, 1/*maxAddr*/,
#if ASYN_VERSION == 4 && ASYN_REVISION < 32
                     numParams/*NUM_PARAMS*/,
#endif
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynEnumMask | 
                     asynInt8ArrayMask | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat32ArrayMask | asynFloat64ArrayMask | asynDrvUserMask, /*Interface mask*/
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynEnumMask | 
                     asynInt8ArrayMask | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat32ArrayMask | asynFloat64ArrayMask, /* Interrupt mask */
                     0/*asynFlags*/,
                     1/*autoConnect*/, 
                     0/*priority*/, 
                     0/*stackSize*/), pModule(NULL), pFuncRead(NULL), pFuncReadEnum(NULL), pFuncWrite(NULL), pThreadState(NULL)
{
    /* Initialize Python and thread support */
    if (!Py_IsInitialized()) {
        Py_Initialize();
        PyEval_InitThreads();
        this->pThreadState = PyThreadState_Get();
    } else {
        PyEval_AcquireLock();
        this->pThreadState = Py_NewInterpreter();
    }

    /* Create extenion module */
    #if PY_MAJOR_VERSION >= 3
    PyObject *pParams = PyModule_Create(&ParamModule);
    #else
    PyObject *pParams = Py_InitModule("param", ParamMethods);
    #endif
    PyModule_AddObject(pParams,      "this",          CAPSULE_BUILD((void*)this, "asynPythonDriver", NULL));
    PyModule_AddIntConstant(pParams, "Int32",         asynParamInt32);
    PyModule_AddIntConstant(pParams, "UInt32Digital", asynParamUInt32Digital);
    PyModule_AddIntConstant(pParams, "Float64",       asynParamFloat64);
    PyModule_AddIntConstant(pParams, "Octet",         asynParamOctet);
    PyModule_AddIntConstant(pParams, "Int8Array",     asynParamInt8Array);
    PyModule_AddIntConstant(pParams, "Int16Array",    asynParamInt16Array);
    PyModule_AddIntConstant(pParams, "Int32Array",    asynParamInt32Array);
    PyModule_AddIntConstant(pParams, "Float32Array",  asynParamFloat32Array);
    PyModule_AddIntConstant(pParams, "Float64Array",  asynParamFloat64Array);

    /* Import user module and locate dirver support methods */
    pModule = PyImport_ImportModule(moduleName);
    if (pModule == NULL) {
        PyErr_Print();
    } else {
        PyObject *pObjectDriver = PyObject_GetAttrString(pModule, "driver");
        if (pObjectDriver == NULL) {
             PyErr_Print();
        } else {
            pFuncRead = PyObject_GetAttrString(pObjectDriver, "read");
            if (pFuncRead == NULL) {
                PyErr_Print();
            }
            pFuncReadEnum = PyObject_GetAttrString(pObjectDriver, "readEnum");
            if (pFuncReadEnum == NULL) {
                PyErr_Print();
            }
            pFuncWrite = PyObject_GetAttrString(pObjectDriver, "write");
            if (pFuncWrite == NULL) {
                PyErr_Print();
            }
        }
    }

    /* We are done with Python interpreter in main thread */
    PyEval_SaveThread();
}

/* The dtor is nowhere called when IOC exits, thus untested. */
asynPythonDriver::~asynPythonDriver()
{
    if (this->pThreadState) {
        PyEval_RestoreThread(this->pThreadState);
        Py_EndInterpreter(this->pThreadState);
        PyEval_ReleaseLock();
    }
}



/* Configuration routine.  Called directly, or from the iocsh function below */

/** EPICS iocsh callable function to call constructor for the asynPythonDriver class.
 * \param[in] portName The name of the asyn port driver to be created.
 * \param[in] moduleName The python module to import.
 * \param[in] numParams Number of parameters in this driver.
 */
static int asynPythonDriverConfigure(const char *portName, const char *moduleName, int numParams)
{
    new asynPythonDriver(portName, moduleName, numParams);
    return(asynSuccess);
}

/* EPICS iocsh shell commands */
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "moduleName",iocshArgString};
static const iocshArg initArg2 = { "numParams", iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2};
static const iocshFuncDef initFuncDef = {"asynPythonDriverConfigure",3,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    asynPythonDriverConfigure(args[0].sval, args[1].sval, args[2].ival);
}

void asynPythonDriverRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}


extern "C" {
epicsExportRegistrar(asynPythonDriverRegister);
}

