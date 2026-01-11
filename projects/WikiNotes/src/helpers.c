#include "../include/helpers.h"
#include <stdlib.h>

int findInArr(size_t target, size_t *arr, size_t arrSize) {
  for (int i = 0; i < arrSize; i++) {
    if (arr[i] == target) {
      return i;
    }
  }
  return -1;
}
