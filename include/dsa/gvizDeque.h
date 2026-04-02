#ifndef _GVIZ_QUEUE_H_
#define _GVIZ_QUEUE_H_

#include <stddef.h>
typedef struct gvizDeque {
  void *arr;
  void *begin;
  size_t count;
  size_t capacity;
  size_t elementSize;
} gvizDeque;

int gvizDequeInit(gvizDeque *d, size_t elementSize);
int gvizDequeInitAtCapacity(gvizDeque *d, size_t elementSize,
                            size_t initialCapacity);

int gvizDequeIsEmpty(const gvizDeque *d);
size_t gvizDequeSize(const gvizDeque *d);
void *gvizDequeAtIndex(const gvizDeque *d, size_t idx);

void gvizDequePopLeft(gvizDeque *d, void *res);
void gvizDequePopRight(gvizDeque *d, void *res);

void *gvizDequePeekLeft(const gvizDeque *d);
void *gvizDequePeekRight(const gvizDeque *d);

int gvizDequePush(gvizDeque *d, void *item);

void gvizDequeRelease(gvizDeque *d);

#endif
