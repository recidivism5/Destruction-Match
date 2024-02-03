#include <alutil.h>

ALenum alCheckError_(char *file, int line){
    ALenum errorCode = alGetError();
    if(errorCode != AL_NO_ERROR){
        char *error;
        switch(errorCode)
        {
            case AL_INVALID_NAME:
                error = "AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function";
                break;
            case AL_INVALID_ENUM:
                error = "AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function";
                break;
            case AL_INVALID_VALUE:
                error = "AL_INVALID_VALUE: an invalid value was passed to an OpenAL function";
                break;
            case AL_INVALID_OPERATION:
                error = "AL_INVALID_OPERATION: the requested operation is not valid";
                break;
            case AL_OUT_OF_MEMORY:
                error = "AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory";
                break;
            default:
                error = "UNKNOWN AL ERROR";
        }
        fatal_error("%s %s (%d)",error,file,line);
    }
    return errorCode;
}