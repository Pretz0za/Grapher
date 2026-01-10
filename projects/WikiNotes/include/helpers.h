#ifndef WIKIHELPERS
#define WIKIHELPERS

#include "./parser.h"

int pushToRefs(Note *n, size_t other);
int findInArr(size_t target, size_t *arr, size_t arrSize);

#endif
