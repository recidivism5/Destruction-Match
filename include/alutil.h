#include <base.h>

#include <AL/al.h>
#include <AL/alc.h>

ALenum alCheckError_(char *file, int line);
#define alCheckError() alCheckError_(FILENAME, __LINE__)