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
- **gvizGraphVBO** (`include/renderer/layers/gvizGraphVBO.h`, `src/renderer/layers/gvizGraphVBO.c`): Shared GPU buffer helper used by both `gvizLayerGraph` and `gvizLayerTutte`. Builds an expanded (non-indexed) VBO of edge endpoints (`float[3]` per endpoint, 2 per directed neighbor pair) and draws via `glDrawArrays(GL_LINES)`. Uses rlgl for VAO/VBO management and the default raylib shader with manual MVP matrix composition. `gpuDirty` flag (2=topology rebuild, 1=positions-only upload via `glBufferSubData`, 0=clean) drives update cost. The `|| !vbo.vaoId` guard in the draw path ensures a full rebuild if update ran before the first draw frame.
- **Polymorphic embedding ownership**: `gvizLayerGraph` holds a `gvizEmbeddedGraph *` that may point directly into any algorithm state struct (e.g. `gvizGRIPState`, `gvizEmbeddedTree`) since all have `gvizEmbeddedGraph` as their first member. A `releaseGraph` function pointer on the layer handles algorithm-specific teardown; `NULL` means borrowed (no ownership).
- **Platform**: macOS, OpenGL 3.3 Core Profile. Raw GL calls use `<OpenGL/gl3.h>` with `#define GL_SILENCE_DEPRECATION`. rlgl's `rlDrawVertexArray*` functions hardcode `GL_TRIANGLES` so `glDrawArrays` is called directly for lines.

### 4. Utilities (`src/utils/`, `include/utils/`)
- **graphs.c**: Generates test graphs (Sierpinski triangles/tetrahedrons, grid meshes).
- **serializers.c**: Saves/loads `Embedding` structs to disk.
- **helpers.c**: Coordinate math, neighbor utilities, screen output.

### 5. GUI (IMPORTANT: In progress):
Start with this analogy. An infinite whiteboard app, like miro or figma, where objects live on the plane in specific positions and scales. the user can pan/zoom. The same should be of this GUI, but also supporting 3D. Consider creating a gvizScene struct that hold all objects, defines 2D or 3D world/camera, manages input and propogating to the correct objects. Objects are gvizLayer structs. they can be graphs, but also buttons, sliders, text, etc. HUD, sidebar, where the camera framebuffer renders to (By default this should be the whole screen but say for a debug view of algorithms it can be useful), and so on.

Creating scnenes should NOT take a lot of code, and me mostly initializing and calling appropriate functions for type of scene and graph to be rendered.

After opening a default window, the following should happen:
- **main menu**: A menu comes up with options to 1) load a scene from disk, 2) open a demo scene (e.g. GRIP of a sierpinski triangle). 3) start a blank scene.
- **scene**: Load the appropriate scene and graphs (if any) and render. The user can interact with the scene and graph just the analogy.
- **new graph**: In an empty scene (or none empty one), the user can load a graph from disk and create an additional layer in the scene for it. Or they can command click to add vertices and edges (by clicking nothing or clicking two vertices, respectively). While the graph is being created, use the tutte realtime embedding! 
- **changing embeddings**: The default embedding for a new graph layer is tutte, loaded graphs are different more on it later. Right clicking a graph layer on the screen creates a menu with options to change the embedding (tutte, GRIP, planar, etc). When the user selects a new embedding, the current embedding is released and the new one is initialized. If the embedding requires a kuratowski subdivision (e.g. planar), then this should be computed and stored in the `gvizPlanarEmbeddingState` struct for reuse in the future if the user switches back to planar embedding. The new embedding is then rendered in place of the old one. If the user switches back to an embedding that requires a kuratowski subdivision, the precomputed one is reused.

### 6. Loading and Saving:
- THE ONLY SUPPORTED GRAPH TYPE IS TREES. DONT IMPLEMENT ANY OTHER GRAPH SERIALIZATION AND IGNORE EXISTING ONES.
- **Trees**: Trees are saved as JSON files with the tree structure. Use jq to parse and generate these files. The file should contain the vertex data (if any) and the edges (parent-child relationships). Parsing should create the gvizGraph that is a directed tree with a specific root node.

- ### Key types

```c
typedef struct { void *data; size_t elem_size; size_t length; size_t capacity; } gvizArray;
typedef struct { void *data; gvizArray neighbors; } gvizVertex;
typedef struct { gvizArray vertices; size_t *map; int directed; } gvizGraph;
typedef struct { size_t dim; double *vertexPositions; } gvizEmbedding;

/*
 * Non-materializing view over a graph. Borrows the graph; never mutates
 * it. Optional vertex / edge bit masks select a subset; NULL masks mean
 * "everything". `edgeStart` is a per-vertex prefix sum into `edgeMask`.
 * See include/dsa/gvizGraphView.h for the iterator + mutation interface.
 */
typedef struct {
  gvizGraph *graph;
  GVIZ_BIT_ARRAY vertexMask;
  GVIZ_BIT_ARRAY edgeMask;
  size_t *edgeStart;
  size_t count;
  size_t edgeStartStale;
} gvizGraphView;

typedef enum { GVIZ_EMBED_FULL_GRAPH, GVIZ_EMBED_VIEW_ONLY } gvizEmbeddingMode;

/*
 * View-aware embedded graph. The view is owned (its mask + edgeStart
 * memory). `mode` chooses whether `embedding.vertexPositions` is sized to
 * the full underlying graph (Mode A, raw indexing) or to view.count
 * (Mode B, with `local` mapping raw vertex id -> local index).
 */
typedef struct {
  gvizGraphView view;
  gvizEmbeddingMode mode;
  size_t *local;           /* raw vertex id -> local index in Mode B */
  gvizEmbedding embedding;
} gvizEmbeddedGraph;

typedef struct { gvizEmbeddedGraph embedding; gvizGraph *kuratowskiSubdivision; } gvizPlanarEmbeddingState;
```

All embedding algorithms expose a canonical `*InitView(state, view, ...)`
entry point that takes ownership of the view. Legacy `*Init(state, graph,
...)` shims remain as thin wrappers that build a Full view internally —
slated for removal in a future cleanup pass.

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
