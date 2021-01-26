#ifndef CHUNK_FORMAT_H
#define CHUNK_FORMAT_H

#include "git-compat-util.h"

struct hashfile;
struct chunkfile;

struct chunkfile *init_chunkfile(struct hashfile *f);
void free_chunkfile(struct chunkfile *cf);
int get_num_chunks(struct chunkfile *cf);
typedef int (*chunk_write_fn)(struct hashfile *f,
			      void *data);
void add_chunk(struct chunkfile *cf,
	       uint64_t id,
	       chunk_write_fn fn,
	       size_t size);
int write_chunkfile(struct chunkfile *cf, void *data);

#endif
