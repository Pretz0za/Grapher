#include "utils/serializers.h"
#include <stdio.h>

int gvizSerializeUINT64(void *datum, char *buf, size_t bufsize) {
  return snprintf(buf, bufsize, "%zu", *(size_t *)datum) >= 0;
}
int gvizSerializeINT(void *datum, char *buf, size_t bufsize) {
  return snprintf(buf, bufsize, "%d", *(int *)datum) >= 0;
}
int gvizSerializeCHAR(void *datum, char *buf, size_t bufsize) {
  return snprintf(buf, bufsize, "%c", *(char *)datum) >= 0;
}
int gvizSerializeSTR(void *datum, char *buf, size_t bufsize) {
  return snprintf(buf, bufsize, "%s", (char *)datum) >= 0;
}
