#include "utils/gvizOBJLoader.h"
#include "core/alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_SIZE 4096
#define MAX_FACE_VERTS 64
#define OUTER_FACE_CAP 8

static int parseFaceIndex(const char *tok, long *out) {
    while (*tok == ' ' || *tok == '\t') tok++;
    if (*tok == '\0') return -1;
    char *end = NULL;
    long v = strtol(tok, &end, 10);
    if (end == tok) return -1;
    *out = v;
    return 0;
}

int gvizLoadOBJAsGraph(const char *path, gvizGraph *outGraph,
                       size_t outerTriangle[8], size_t *outerFaceLen) {
    if (!path || !outGraph || !outerTriangle || !outerFaceLen) return -1;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[OBJ] FAILED: cannot open %s\n", path);
        return -1;
    }

    char *line = (char *)GVIZ_ALLOC(MAX_LINE_SIZE);
    if (!line) {
        fprintf(stderr, "[OBJ] FAILED: line buffer alloc\n");
        fclose(fp);
        return -1;
    }

    size_t vertexCount = 0;
    while (fgets(line, MAX_LINE_SIZE, fp)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) vertexCount++;
    }

    fprintf(stderr, "[OBJ] vertex count: %zu\n", vertexCount);

    if (vertexCount == 0) {
        fprintf(stderr, "[OBJ] FAILED: zero vertices\n");
        GVIZ_DEALLOC(line);
        fclose(fp);
        return -1;
    }

    if (gvizGraphInit(outGraph, 0) != 0) {
        fprintf(stderr, "[OBJ] FAILED: graph init\n");
        GVIZ_DEALLOC(line);
        fclose(fp);
        return -1;
    }
    for (size_t i = 0; i < vertexCount; i++) {
        if (gvizGraphAddVertex(outGraph, NULL, NULL, NULL) != 0) {
            fprintf(stderr, "[OBJ] FAILED: add vertex %zu\n", i);
            gvizGraphRelease(outGraph);
            GVIZ_DEALLOC(line);
            fclose(fp);
            return -1;
        }
    }

    rewind(fp);

    int gotOuter = 0;
    long faceVerts[MAX_FACE_VERTS];
    size_t faceCounter = 0;

    while (fgets(line, MAX_LINE_SIZE, fp)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (p[0] == '#' || p[0] == '\0' || p[0] == '\n' || p[0] == '\r')
            continue;
        if (!(p[0] == 'f' && (p[1] == ' ' || p[1] == '\t'))) continue;

        p += 2;
        size_t faceLen = 0;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '\n' || *p == '\r') break;

            char tokBuf[64];
            size_t ti = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' &&
                   *p != '/' && ti + 1 < sizeof(tokBuf)) {
                tokBuf[ti++] = *p++;
            }
            tokBuf[ti] = '\0';
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                p++;

            long idx;
            if (parseFaceIndex(tokBuf, &idx) != 0) {
                fprintf(stderr, "[OBJ] FAILED: parse face index '%s'\n", tokBuf);
                gvizGraphRelease(outGraph);
                GVIZ_DEALLOC(line);
                fclose(fp);
                return -1;
            }
            if (idx < 0) idx = (long)vertexCount + idx + 1;
            if (idx <= 0 || (size_t)idx > vertexCount) {
                fprintf(stderr, "[OBJ] FAILED: face index out of range %ld\n", idx);
                gvizGraphRelease(outGraph);
                GVIZ_DEALLOC(line);
                fclose(fp);
                return -1;
            }
            if (faceLen < MAX_FACE_VERTS)
                faceVerts[faceLen++] = idx - 1;
        }

        if (faceLen < 3) continue;

        fprintf(stderr, "[OBJ] face %zu: len=%zu verts=[%ld %ld %ld %ld]\n",
                faceCounter, faceLen,
                faceLen > 0 ? faceVerts[0] : -1,
                faceLen > 1 ? faceVerts[1] : -1,
                faceLen > 2 ? faceVerts[2] : -1,
                faceLen > 3 ? faceVerts[3] : -1);

        if (!gotOuter) {
            size_t cap = faceLen < OUTER_FACE_CAP ? faceLen : OUTER_FACE_CAP;
            for (size_t k = 0; k < cap; k++)
                outerTriangle[k] = (size_t)faceVerts[k];
            *outerFaceLen = cap;
            gotOuter = 1;
            fprintf(stderr, "[OBJ] outer face verts:");
            for (size_t k = 0; k < cap; k++)
                fprintf(stderr, " %zu", outerTriangle[k]);
            fprintf(stderr, "\n");
        }

        for (size_t i = 0; i < faceLen; i++) {
            size_t a = (size_t)faceVerts[i];
            size_t b = (size_t)faceVerts[(i + 1) % faceLen];
            if (a == b) continue;
            if (!gvizGraphEdgeExists(outGraph, a, b))
                gvizGraphAddEdge(outGraph, a, b);
        }

        fprintf(stderr, "[OBJ] edges after face %zu: total graph verts ~%zu\n",
                faceCounter, outGraph->vertices.count);
        faceCounter++;
    }

    GVIZ_DEALLOC(line);
    fclose(fp);

    if (!gotOuter) {
        fprintf(stderr, "[OBJ] FAILED: no faces found\n");
        gvizGraphRelease(outGraph);
        return -1;
    }
    fprintf(stderr, "[OBJ] done. vertices=%zu\n", vertexCount);
    return 0;
}
