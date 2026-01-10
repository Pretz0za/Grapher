#ifndef STACK
#define STACK

#include <stddef.h>
#include <stdio.h>

typedef struct {
  size_t *arr;
  size_t count;
  size_t capacity;
} Vector;

[[nodiscard]] Vector *createVec();
int pushToVec(Vector *v, size_t item);
size_t popVec(Vector *v);
int deleteFromVec(Vector *v, size_t item);
int isEmpty(Vector *v);
size_t *findInVec(Vector *v, size_t item);

int copyVec(Vector *dest, Vector *src);
void printVec(Vector *v, FILE *stream);

void destroyVec(Vector *v);

#endif
