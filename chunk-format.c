#include "cache.h"
#include "chunk-format.h"
#include "csum-file.h"
#define CHUNK_LOOKUP_WIDTH 12

/*
 * When writing a chunk-based file format, collect the chunks in
 * an array of chunk_info structs. The size stores the _expected_
 * amount of data that will be written by write_fn.
 */
struct chunk_info {
	uint32_t id;
	uint64_t size;
	chunk_write_fn write_fn;
};

struct chunkfile {
	struct hashfile *f;

	struct chunk_info *chunks;
	size_t chunks_nr;
	size_t chunks_alloc;
};

struct chunkfile *init_chunkfile(struct hashfile *f)
{
	struct chunkfile *cf = xcalloc(1, sizeof(*cf));
	cf->f = f;
	return cf;
}

void free_chunkfile(struct chunkfile *cf)
{
	if (!cf)
		return;
	free(cf->chunks);
	free(cf);
}

int get_num_chunks(struct chunkfile *cf)
{
	return cf->chunks_nr;
}

void add_chunk(struct chunkfile *cf,
	       uint64_t id,
	       chunk_write_fn fn,
	       size_t size)
{
	ALLOC_GROW(cf->chunks, cf->chunks_nr + 1, cf->chunks_alloc);

	cf->chunks[cf->chunks_nr].id = id;
	cf->chunks[cf->chunks_nr].write_fn = fn;
	cf->chunks[cf->chunks_nr].size = size;
	cf->chunks_nr++;
}

int write_chunkfile(struct chunkfile *cf, void *data)
{
	int i;
	size_t cur_offset = cf->f->offset + cf->f->total;

	/* Add the table of contents to the current offset */
	cur_offset += (cf->chunks_nr + 1) * CHUNK_LOOKUP_WIDTH;

	for (i = 0; i < cf->chunks_nr; i++) {
		hashwrite_be32(cf->f, cf->chunks[i].id);
		hashwrite_be64(cf->f, cur_offset);

		cur_offset += cf->chunks[i].size;
	}

	/* Trailing entry marks the end of the chunks */
	hashwrite_be32(cf->f, 0);
	hashwrite_be64(cf->f, cur_offset);

	for (i = 0; i < cf->chunks_nr; i++) {
		uint64_t start_offset = cf->f->total + cf->f->offset;
		int result = cf->chunks[i].write_fn(cf->f, data);

		if (result)
			return result;

		if (cf->f->total + cf->f->offset != start_offset + cf->chunks[i].size)
			BUG("expected to write %"PRId64" bytes to chunk %"PRIx32", but wrote %"PRId64" instead",
			    cf->chunks[i].size, cf->chunks[i].id,
			    cf->f->total + cf->f->offset - start_offset);
	}

	return 0;
}
