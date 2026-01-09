#include "../../include/graph.h"
#include "./include/parser.h"
#include <stdlib.h>

// TODO:
// 1- Loop through input text and create notes arr.
// 2- Create Graph struct with appropriate size.
// 3- Loop through notes and create vertices.
// 4- Loop through notes' body and add edges.

Graph *makeNotesGraph(Note **notes);

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(1);
}
