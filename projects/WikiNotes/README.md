Given a list of a Wikipedia page's Notes section (one per line, in a file),
creates a graph where vertices are notes and edges are cross-references. Can
take a filter to only consider a subset of the notes. The program also
performs a DFS of the graph from any starting vertex and prints the DFS tree. It
does it again on the reverse graph which checks whether the graph of notes is
stronly connected.

For COMPSCI 163 PSET #1, we are asked whether the notes subset of m, r, s, t, ab
, ad, ai, al, am, ar, bz, ca, cf, ci, cj, ck, cl, cm, ct, cu, cv, cy, cz, and dd
is strongly connected or not. Both DFS trees reach all 24 vertices, thus it is.
