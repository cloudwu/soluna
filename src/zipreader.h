#ifndef soluna_zip_reader_h
#define soluna_zip_reader_h

#include <stdint.h>

struct zipreader_name {
	const char * zipfile;
	const char * root;
	size_t root_size;
};

typedef void * zipreader_file;

zipreader_file zipreader_open(struct zipreader_name *names, const char * filename);
void zipreader_close(zipreader_file f);
int zipreader_read(zipreader_file f, void *dst, int bytes);
int zipreader_seek(zipreader_file f, int64_t offset, int origin);
int64_t zipreader_tell(zipreader_file f);
size_t zipreader_size(zipreader_file f);

#endif
