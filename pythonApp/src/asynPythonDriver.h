#ifndef asynPythonDriver_H
#define asynPythonDriver_H

#include "asynPortDriver.h"
#include <shareLib.h>

class /*epicsShareFunc*/ asynPythonDriver : public asynPortDriver {
public:
    asynPythonDriver(const char *portName, const char *pythonFileName, int numParams);
    ~asynPythonDriver();

    /* These are the methods that we override from asynPortDriver */
    
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    virtual asynStatus readEnum(asynUser *pasynUser, char *strings[], int values[], int severities[], size_t nElements, size_t *nIn);

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);

    template <typename epicsType> asynStatus readArray(asynUser *pasynUser, epicsType *value, size_t nElements, size_t *nIn);

    virtual asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value, size_t nElements, size_t *nIn);
    virtual asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn);

    virtual void report(FILE *fp, int details);

    /* These are the methods that are new to this class */

protected:
    PyObject *pModule, *pFuncRead, *pFuncReadEnum, *pFuncWrite;
    PyThreadState *pThreadState;
};
#endif
