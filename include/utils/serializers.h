#ifndef GVIZ_SERIALIZERS_H
#define GVIZ_SERIALIZERS_H

#include <stddef.h>

/** Serializes one element into @p buf; returns bytes written, or -1 on failure. */
typedef int gvizSerializeDatum(void *datum, char buf[], size_t bufsize);

/** Serializes a uint64_t. */
int gvizSerializeUINT64(void *datum, char buf[], size_t bufsize);

/** Serializes an int. */
int gvizSerializeINT(void *datum, char buf[], size_t bufsize);

/** Serializes a char. */
int gvizSerializeCHAR(void *datum, char buf[], size_t bufsize);

/** Serializes a C string pointer. */
int gvizSerializeSTR(void *datum, char buf[], size_t bufsize);

#endif
