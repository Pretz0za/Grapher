#ifndef GVIZ_CORE_ALLOC_H
#define GVIZ_CORE_ALLOC_H

/*
 * Allocation macros used by every gviz allocation. Define GVIZ_ALLOC,
 * GVIZ_REALLOC, and GVIZ_DEALLOC before including any gviz header to plug in
 * a custom allocator; all three must be overridden together.
 *
 * Note: buffers produced by POSIX getline() are always released with plain
 * free() regardless of these macros, since getline allocates with malloc.
 */

#include <stdlib.h>

#ifndef GVIZ_ALLOC
#define GVIZ_ALLOC(x) malloc(x)
#endif

#ifndef GVIZ_REALLOC
#define GVIZ_REALLOC(x, y) realloc(x, y)
#endif

#ifndef GVIZ_DEALLOC
#define GVIZ_DEALLOC(x) free(x)
#endif

#endif
