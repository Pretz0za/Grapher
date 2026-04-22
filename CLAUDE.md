# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
mkdir -p build && cd build
cmake ..
make
```

Enable AddressSanitizer for memory debugging:
```bash
cmake .. -DENABLE_ASAN=ON
```

## Testing

```bash
# Run all tests
cd build && ctest

# Run a single test executable
./build/tests/dsa/gvizArrayTests
./build/tests/dsa/gvizGraphTests
./build/tests/dsa/gvizDequeTests
./build/tests/dsa/gvizBitArrayTests
./build/tests/renderer/GRIP/gvizGRIPTests
./build/tests/renderer/planar/gvizPlanarTests

# Interactive demos These show how scene is created and camera is controller in 2D/3D space.
./build/tests/renderer/treeDemo
./build/tests/renderer/gripDemo
```

Tests use the Unity framework (`/third-party/unity/`).

## Architecture

Grapher is a C11 graph visualization library. The static library (`libgraphvis.a`) is built from four main layers:

### 1. Data Structures (`src/dsa/`, `include/dsa/`)

All Data Structures are implemented from scratch in `src/dsa/` and exposed via `include/dsa/`. Implemented structures are:
- **gvizGraph**: Adjacency-list graph (directed/undirected). Vertices stored in a `gvizArray` that auto-expands from a default capacity of 64.
- **gvizArray**: Type-erased dynamic array (stores element size at runtime).
- **gvizDeque**, **gvizBitArray**, **gvizTree**: Supporting structures for BFS/DFS, bitwise operations, and tree validation.

### 2. Embedding Algorithms (`src/renderer/embeddings/`, `include/renderer/embeddings/`)
The base tyoe is the `gvizEmbeddedGraph` that holds a `gvizGraph *` and `gvizEmbedding`, which is wrapped with the various types of embedding algorithms and their states.
Each algorithm (with its set of intitializing and helper functions) encapsulates a `gvizEmbeddedGraph` and produces an `gvizEmbeddingGraph` holding an`Embedding` (n-dimensional array of `double` positions in the world):

`gvizReingoldTilfordAlgorithm` Reingold-Tilford algorithm. Used for Trees — hierarchical, level-aligned.
`gvizGRIPEmbedding` GRIP (maximal independent set reduction + forces) Large graphs (2M+ vertices)
`gvizPlanarEmbedding` Kuratowski subdivision detection + rotation system Planar graphs detection. Third-party code.
`gvizSchnyderWood` Schnyder wood decomposition Planar straight-line drawings (in progress...)
`gvizForceDirected` General force algorithms for graphs. Kamada-Kawai / Fruchterman-Reingold for GRIP implemented. (in progress...)

### 4. Position and Physics Vectors
All vector math is done using cblas vectors (double *) for flexibility and performance. 

### 3. Renderer (`src/renderer/`, `include/renderer/`)
- **gvizRenderer**: Wraps Raylib for windowed drawing. Manages a `VisState` containing a view stack for zoom/pan.
- **gvizLayer / gvizLayerGraph**: Abstraction for multi-layer rendering with viewport management. Draws a `gvizEmbeddedGraph`.

### 4. Utilities (`src/utils/`, `include/utils/`)
- **graphs.c**: Generates test graphs (Sierpinski triangles/tetrahedrons, grid meshes).
- **serializers.c**: Saves/loads `Embedding` structs to disk.
- **helpers.c**: Coordinate math, neighbor utilities, screen output.

### Key types

```c
typedef struct { void *data; size_t elem_size; size_t length; size_t capacity; } gvizArray;
typedef struct { void *data; gvizArray neighbors; } gvizVertex;
typedef struct { gvizArray vertices; size_t *map; int directed; } gvizGraph;
typedef struct { gvizGraph *graph; double *embedding; size_t dim; } gvizEmbeddedGraph;
typeded struct { gvizEmbeddedGraph embedding; gvizGraph *kuratowskiSubdivision; } gvizPlanarEmbeddingState;
```

### Memory
Allocation macros are in `include/core/alloc.h` (`GVIZ_ALLOC`, `GVIZ_REALLOC`, `GVIZ_DEALLOC`) — override to plug in a custom allocator.

### Style
- Any embedding that requires its own struct for state (e.g. GRIP, Planar) should have its own file with its initializer, releaser, and interface.
- All allocations must be paired with deallocations. Use `GVIZ_ALLOC` / `GVIZ_REALLOC` for allocs, `GVIZ_DEALLOC` for all frees.
- No comments in the .c files — all public interfaces must be documented in the .h files. Internal functions can be left uncommented if their purpose is clear from the name.
- Keep functions short and focused. If a function exceeds ~70 lines, CONSIDER breaking it into smaller helper functions, unless that is not better.

## Dependencies

- **raylib** — windowing and graphics
- **libplanar** — planar graph primitives
- **libopenblas / LAPACK** — linear algebra (PCA projection in GRIP)
- **msf_gif** — GIF recording in demos
- macOS frameworks: Cocoa, OpenGL, CoreAudio, CoreVideo
