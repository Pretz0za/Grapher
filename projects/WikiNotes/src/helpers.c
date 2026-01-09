#include "../include/helpers.h"
#include "../include/parser.h"
#include <stdlib.h>

int pushToRefs(Note *n, size_t other) {
  if (!(n->count < n->capacity)) {
    long newSize = sizeof(size_t) * (n->capacity ? n->capacity * 2 : 4);
    size_t *newArr = realloc(n->references, newSize);
    if (!newArr) {
      return 1;
    }
    n->references = newArr;
  }
  n->references[n->count++] = other;
  return 0;
}
