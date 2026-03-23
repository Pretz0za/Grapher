#ifndef GVIZ_ALLOC
#define GVIZ_ALLOC(x) malloc(x)
#endif

#ifndef GVIZ_REALLOC
#define GVIZ_REALLOC(x, y) realloc(x, y)
#endif

#ifndef GVIZ_DEALLOC
#define GVIZ_DEALLOC(x) free(x)
#endif
