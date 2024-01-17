#include <base.h>
#include <whereami.h>

void fatal_error(char *format, ...){
	va_list args;
	va_start(args,format);
	static char msg[1024];
	vsnprintf(msg,COUNT(msg),format,args);
	boxerShow(msg,"Error",BoxerStyleError,BoxerButtonsQuit);
	va_end(args);
	exit(1);
}

void *malloc_or_die(size_t size){
	void *p = malloc(size);
	if (!p) fatal_error("malloc failed.");
	return p;
}

void *zalloc_or_die(size_t size){
	void *p = calloc(1,size);
	if (!p) fatal_error("zalloc failed.");
	return p;
}

void *realloc_or_die(void *ptr, size_t size){
	void *p = realloc(ptr,size);
	if (!p) fatal_error("realloc failed.");
	return p;
}

char *local_path_to_absolute(char *localPath){
	static char absolutePath[4096];
	int rootLen = wai_getExecutablePath(0,0,0);
	int totalLen = rootLen + strlen(localPath) + 1;
	ASSERT(totalLen <= COUNT(absolutePath));
	int dirNameLen;
	wai_getExecutablePath(absolutePath,rootLen,&dirNameLen);
	absolutePath[rootLen] = 0;
	char *lastI = strrchr(absolutePath,'/');
	if (!lastI){
		lastI = strrchr(absolutePath,'\\');
		if (!lastI){
			fatal_error("Invalid exe path: %s",absolutePath);
		}
	}
	lastI[1] = 0;
	strcat(absolutePath,localPath);
	return absolutePath;
}

char *load_file_as_cstring(char *localPath){
	char *absolutePath = local_path_to_absolute(localPath);
	FILE *f = fopen(absolutePath,"rb");
	if (!f){
		fatal_error("Could not open file: %s",local_path_to_absolute(localPath));
	}
	fseek(f,0,SEEK_END);
	long len = ftell(f);
	ASSERT(len > 0);
	fseek(f,0,SEEK_SET);
	char *str = malloc_or_die(len+1);
	fread(str,1,len,f);
	str[len] = 0;
	fclose(f);
	return str;
}

int rand_int(int n){
	if ((n - 1) == RAND_MAX){
		return rand();
	} else {
		// Chop off all of the values that would cause skew...
		int end = RAND_MAX / n; // truncate skew
		end *= n;
		// ... and ignore results from rand() that fall above that limit.
		// (Worst case the loop condition should succeed 50% of the time,
		// so we can expect to bail out of this loop pretty quickly.)
		int r;
		while ((r = rand()) >= end);
		return r % n;
	}
}

int rand_int_range(int min, int max){
	return rand_int(max-min+1) + min;
}

uint32_t fnv_1a(int keylen, char *key){
	uint32_t index = 2166136261u;
	for (int i = 0; i < keylen; i++){
		index ^= key[i];
		index *= 16777619;
	}
	return index;
}

int modulo(int i, int m){
	return (i % m + m) % m;
}