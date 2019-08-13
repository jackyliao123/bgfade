#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdbool.h>

struct vector {
	void *entries;
	void *tmp;
	size_t entry_size;
	size_t size;
	size_t capacity;
};

void vector_create(struct vector *v, size_t entry_size, size_t capacity);
void vector_destroy(struct vector *v);
void *vector_push(struct vector *v, void *entry);
void *vector_getptr(struct vector *v, size_t ind);
void *vector_alloc(struct vector *v);
void vector_swap(struct vector *v, size_t a, size_t b);

#endif
