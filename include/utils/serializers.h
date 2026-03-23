#ifndef __GVIZ_SERIALIZERS__
#define __GVIZ_SERIALIZERS__

#include <stddef.h>

typedef int gvizSerializeDatum(void *datum, char buf[], size_t bufsize);

int gvizSerializeUINT64(void *datum, char buf[], size_t bufsize);
int gvizSerializeINT(void *datum, char buf[], size_t bufsize);
int gvizSerializeCHAR(void *datum, char buf[], size_t bufsize);
int gvizSerializeSTR(void *datum, char buf[], size_t bufsize);

#endif
