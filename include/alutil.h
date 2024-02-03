#include <base.h>
#include <linmath.h>

#include <AL/al.h>
#include <AL/alc.h>

bool alCheckError_(char *file, int line);
#define alCheckError() alCheckError_(FILENAME,__LINE__)

bool alcCheckError_(char *file, int line, ALCdevice *device);
#define alcCheckError(device) alcCheckError_(FILENAME,__LINE__,device)

ALuint load_sound(char *name);

void init_sound_sources(void);

void play_sound(ALuint id, vec3 position);