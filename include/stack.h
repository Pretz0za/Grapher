#ifndef STACK
#define STACK

#include <stddef.h>
#include <stdio.h>

/**
 * @brief A dynamically allocated vector of size_t.
 *
 * This struct manages data for maintaining a dynamically sized and allocated
 * vector of unsigned longs (size_t). The memory pointed to by arr are owned
 * be the Vector, and are freed using destroyVec().
 */

typedef struct {
  size_t *arr;     /**< Underlying array of data */
  size_t count;    /**< Number of elements currently in arr */
  size_t capacity; /**< Maximum capacity of currently allocated arr */
} Vector;

/**
 * Allocates memory for a Vector and returns it. The called is responsible for
 * freeing the memory using destroyVec()
 *
 * @return A pointer to the newly allocated Vector
 */
[[nodiscard]] Vector *createVec();

/**
 * Pushes an item to the end of a Vector, reallocates if necessary.
 *
 * @param v a pointer to the Vector the item will be pushed to.
 * @param item the value of the item to be added to @p v.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the item was added succesfully.
 * @retval 1 If the reallocation fails. @p v is still valid.
 */
int pushToVec(Vector *v, size_t item);

/**
 * Deletes and returns the last element in a Vector.
 *
 * @param v A pointer to the Vector the item will be deleted from.
 *
 * @return The element deleted from the array
 * @retval value The value of the deleted itm
 * @retval 0     If @p v is empty
 */
size_t popVec(Vector *v);

/**
 * Deletes the first instance of an item from a Vector.
 *
 * @param v    A pointer to the Vector to delete from.
 * @param item The value of the item to delete.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0 If the item was deleted successfully.
 * @retval 1 If the item was not found in the Vecotr.
 */
int deleteFromVec(Vector *v, size_t item);

/**
 * Checks if a Vector is empty.
 *
 * @param v A pointer to the Vector to check.
 *
 * @return An integer signaling the result.
 * @retval 0 The vector is not empty.
 * @retval 1 The vector is empty.
 */
int isEmpty(Vector *v);

/**
 * Finds the first instance of an item in a Vector. O(n)
 *
 * @param v    A pointer to the Vector to search.
 * @param item The value of the item to search for.
 *
 * @return the index of item in @p v.
 */
size_t *findInVec(Vector *v, size_t item);

/**
 * Copies the data of one Vector to another.
 *
 * @param dest A pointer to the Vector the data will be copied to.
 * @param src  A pointer to the Vector the data will be compied from.
 *
 * @return An integer showing whether or no the operation was successful.
 * @retval 0 If the data was copied successfully.
 * @retval 1 If the reallocation of memory failed. Both @p dest and @p src are
 *           still valid.
 */
int copyVec(Vector *dest, Vector *src);

/**
 * Writes the data of a Vector to a stream.
 *
 * The output is formatted like so:
 *   [ item1, item2, ..., itemN, ]
 *
 * @param v      A pointer to the Vector to be printed.
 * @param stream A pointer to the output stream.
 */
void printVec(Vector *v, FILE *stream);

/**
 * Frees a Vector and the memory owned by it.
 *
 * @param v The Vector to be destroyed. This pointer will become invalid after
 *          the function returns
 */
void destroyVec(Vector *v);

#endif
