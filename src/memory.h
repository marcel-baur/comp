#ifndef comp_memory_h
#define comp_memory_h

#include "common.h"
#include "value.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
	(type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void free_objects();

void mark_object(Obj *object);

void mark_value(Value value);

void collect_garbage();

#endif
