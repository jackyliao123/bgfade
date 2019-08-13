#include <string.h>

#include "vector.h"

void vector_create(struct vector *v, size_t entry_size, size_t capacity) {
	v->entries = malloc(capacity * entry_size);
	v->tmp = malloc(entry_size);
	v->entry_size = entry_size;
	v->size = 0;
	v->capacity = capacity;
}

void vector_destroy(struct vector *v) {
	free(v->entries);
	free(v->tmp);
}

void *vector_push(struct vector *v, void *entry) {
	void *a = vector_alloc(v);
	if(entry == NULL) {
		memset(a, 0, v->entry_size);
	} else {
		memcpy(a, entry, v->entry_size);
	}
	return a;
}

void *vector_getptr(struct vector *v, size_t ind) {
	if(ind >= v->size) {
		return NULL;
	}
	return (char *) v->entries + v->entry_size * ind;
}

void *vector_alloc(struct vector *v) {
	if(v->size + 1 > v->capacity) {
		v->capacity = v->capacity * 2;
		v->entries = realloc(v->entries, v->capacity * v->entry_size);
	}
	return (char *) v->entries + v->entry_size * (v->size ++);
}

void vector_swap(struct vector *v, size_t a, size_t b) {
	void *va = vector_getptr(v, a);
	void *vb = vector_getptr(v, b);
	memcpy(v->tmp, va, v->entry_size);
	memcpy(va, vb, v->entry_size);
	memcpy(vb, v->tmp, v->entry_size);
}
