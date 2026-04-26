#include "utils/gvizOBJLoader.h"
#include "core/alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_SIZE 4096
#define MAX_FACE_VERTS 64

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
                       size_t outerTriangle[3]) {
    if (!path || !outGraph || !outerTriangle) return -1;

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char *line = (char *)GVIZ_ALLOC(MAX_LINE_SIZE);
    if (!line) { fclose(fp); return -1; }

    size_t vertexCount = 0;
    while (fgets(line, MAX_LINE_SIZE, fp)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) vertexCount++;
    }

    if (vertexCount == 0) {
        GVIZ_DEALLOC(line);
        fclose(fp);
        return -1;
    }

    if (gvizGraphInit(outGraph, 0) != 0) {
        GVIZ_DEALLOC(line);
        fclose(fp);
        return -1;
    }
    for (size_t i = 0; i < vertexCount; i++) {
        if (gvizGraphAddVertex(outGraph, NULL, NULL, NULL) != 0) {
            gvizGraphRelease(outGraph);
            GVIZ_DEALLOC(line);
            fclose(fp);
            return -1;
        }
    }

    rewind(fp);

    int gotOuter = 0;
    long faceVerts[MAX_FACE_VERTS];

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
                gvizGraphRelease(outGraph);
                GVIZ_DEALLOC(line);
                fclose(fp);
                return -1;
            }
            if (idx < 0) idx = (long)vertexCount + idx + 1;
            if (idx <= 0 || (size_t)idx > vertexCount) {
                gvizGraphRelease(outGraph);
                GVIZ_DEALLOC(line);
                fclose(fp);
                return -1;
            }
            if (faceLen < MAX_FACE_VERTS)
                faceVerts[faceLen++] = idx - 1;
        }

        if (faceLen < 3) continue;

        if (!gotOuter) {
            outerTriangle[0] = (size_t)faceVerts[0];
            outerTriangle[1] = (size_t)faceVerts[1];
            outerTriangle[2] = (size_t)faceVerts[2];
            gotOuter = 1;
        }

        for (size_t i = 0; i < faceLen; i++) {
            size_t a = (size_t)faceVerts[i];
            size_t b = (size_t)faceVerts[(i + 1) % faceLen];
            if (a == b) continue;
            if (!gvizGraphEdgeExists(outGraph, a, b))
                gvizGraphAddEdge(outGraph, a, b);
        }
    }

    GVIZ_DEALLOC(line);
    fclose(fp);

    if (!gotOuter) {
        gvizGraphRelease(outGraph);
        return -1;
    }
    return 0;
}
