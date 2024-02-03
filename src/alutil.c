#include <alutil.h>
#include <stb_vorbis.h>

bool alCheckError_(char *file, int line){
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
        return false;
    }
    return true;
}

bool alcCheckError_(char *file, int line, ALCdevice *device){
    ALCenum error = alcGetError(device);
    if(error != ALC_NO_ERROR){
        switch(error)
        {
            char *error;
            case ALC_INVALID_VALUE:
                error = "ALC_INVALID_VALUE: an invalid value was passed to an OpenAL function";
                break;
            case ALC_INVALID_DEVICE:
                error = "ALC_INVALID_DEVICE: a bad device was passed to an OpenAL function";
                break;
            case ALC_INVALID_CONTEXT:
                error = "ALC_INVALID_CONTEXT: a bad context was passed to an OpenAL function";
                break;
            case ALC_INVALID_ENUM:
                error = "ALC_INVALID_ENUM: an unknown enum value was passed to an OpenAL function";
                break;
            case ALC_OUT_OF_MEMORY:
                error = "ALC_OUT_OF_MEMORY: an unknown enum value was passed to an OpenAL function";
                break;
            default:
                error = "UNKNOWN ALC ERROR";
        }
        fatal_error("%s %s (%d)",error,file,line);
        return false;
    }
    return true;
}

ALuint load_sound(char *name){
    char *path = local_path_to_absolute("res/sounds/%s.ogg",name);
    int channels, sampleRate;
    short *samples;
	int sampleCount = stb_vorbis_decode_filename(path,&channels,&sampleRate,&samples);
    ASSERT(sampleCount > 0);
    ASSERT(channels == 1);
    ALuint id;
    alGenBuffers(1,&id);
    alBufferData(id,AL_FORMAT_MONO16,samples,sampleCount*sizeof(*samples),sampleRate);
    free(samples);
    return id;
}

ALuint sources[256];

void init_sound_sources(void){
    alGenSources(COUNT(sources),sources);
}

void play_sound(ALuint id){
    for (ALuint *s = sources; s < sources+COUNT(sources); s++){
        ALint state;
        alGetSourcei(*s,AL_SOURCE_STATE,&state);
        if (state != AL_PLAYING){
            alSourcef(*s,AL_PITCH,1.0f);
            alSourcef(*s,AL_GAIN,1.0f);
            alSource3f(*s,AL_POSITION,0.0f,0.0f,0.0f);
            alSource3f(*s,AL_VELOCITY,0.0f,0.0f,0.0f);
            alSourcei(*s,AL_LOOPING,AL_FALSE);
            alSourcei(*s,AL_BUFFER,id);
            alSourcePlay(*s);
            return;
        }
    }
    ASSERT(0 && "play_sound overflowed");
}