#ifndef gvizArray_H
#define gvizArray_H

#include "core/alloc.h"
#include "utils/serializers.h"
#include <stddef.h>
#include <stdio.h>

/**
 * @brief A dynamically allocated array of void ptrs.
 *
 * This struct manages data for maintaining a dynamically sized and allocated
 * vector of arbitrary types. The memory pointed to by arr are owned
 * be the Vector, and are freed using gvizArrayRelease(). All copying and
 * equality checking is SHALLOW.
 */
typedef struct {
  void *arr;          /**< Underlying array of data */
  size_t elementSize; /**< Size of one element in the arrayc */
  size_t count;       /**< Number of elements currently in arr */
  size_t capacity;    /**< Maximum capacity of currently allocated arr */
} gvizArray;

/**
 * Allocates memory for an underlying array and initializes @p arr to a
 * gvizArray struct.
 *
 * @param arr A pointer to the memory to initialize. This memory is owned by the
 * caller, and should be freed by the caller.
 *
 * @return An erro code showing whether or not the operation was successful.
 * @retval 0  If the operation was successful.
 * @retval -1 If @p arr is NULL or if the memory allocation for the underlying
 * array fails. @p arr is still valid.
 */
int gvizArrayInit(gvizArray *arr, size_t elementSize);
int gvizArrayInitAtCapacity(gvizArray *arr, size_t elementSize,
                            size_t initialCapacity);

/**
 * Pushes an item to the end of a gvizArray, reallocates if necessary.
 *
 * @param v a pointer to the gvizArray the item will be pushed to.
 * @param item the value of the item to be added to @p v.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the item was added succesfully.
 * @retval -1 If the reallocation fails. @p v is still valid.
 */
int gvizArrayPush(gvizArray *v, void *item);

/**
 * Deletes an item at a given index in O(1) time. Does not preserve the relative
 * ordering of the elements.
 *
 * @param v   A pointer to the gvizArray the item will be deleted from.
 * @param idx The index of the item to delete.
 */
void gvizArraySwapDelete(gvizArray *v, size_t item);

/**
 * Deletes and returns the last element in a gvizArray.
 *
 * @param v   A pointer to the gvizArray the item will be deleted from.
 * @param res A pointer to memory to store the value of the popped element.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful. Result is stored in @p res.
 * @retval -1 If @p v is empty. @p res is not modified.
 */
int gvizArrayPop(gvizArray *v, void *res);

/**
 * Deletes the first instance of an item from a gvizArray.
 *
 * @param v    A pointer to the gvizArray to delete from.
 * @param item The value of the item to delete.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval idx The index of the deleted item in @p v.
 * @retval -1  If the item was not found in the Vecotr.
 */
int gvizArrayFindOneAndDelete(gvizArray *v, void *item);

/**
 * Checks if a gvizArray is empty.
 *
 * @param v A pointer to the gvizArray to check.
 *
 * @return An integer signaling the result.
 * @retval 0 The gvizArray is not empty.
 * @retval 1 The gvizArray is empty.
 */
int gvizArrayIsEmpty(const gvizArray *v);

/**
 * Finds the first instance of an item in a gvizArray. O(n)
 *
 * @param v    A pointer to the gvizArray to search.
 * @param item The value of the item to search for.
 *
 * @return The index of the item in @p v. -1 if not found.
 * @retval idx The index of the item in @p v.
 * @retval -1  If the item was not found in the Vecotr.
 */
int gvizArrayFindOne(const gvizArray *v, void *item);

/**
 * Copies the data of one gvizArray to another.
 *
 * @param dest A pointer to the gvizArray the data will be copied to.
 * @param src  A pointer to the gvizArray the data will be copied from.
 *
 * @return An integer showing whether or no the operation was successful.
 * @retval 0 If the data was copied successfully.
 * @retval -1 If the reallocation of memory failed. Both @p dest and @p src are
 *           still valid.
 */
int gvizArrayCopy(gvizArray *dest, const gvizArray *src);

/**
 * Initalizes an address as an array copies the data of one gvizArray into it.
 *
 * @param dest A pointer to the gvizArray the data will be copied to.
 * @param src  A pointer to the gvizArray the data will be copied from.
 *
 * @return An integer showing whether or no the operation was successful.
 * @retval 0 If the data was copied successfully.
 * @retval -1 If the reallocation of memory failed. Both @p dest and @p src are
 *           still valid.
 */
int gvizArrayClone(gvizArray *dest, const gvizArray *src);

/**
 * Moves the data of one gvizArray to another. After this operation, @p src is
 * invalid and @p dest has taken ownership of the memory that was previously
 * owned by it. The memory previously stored by @p src is lost.
 *
 * @param dest A pointer to the gvizArray the data will be moved to.
 * @param src  A pointer to the gvizArray the data will be moved from.
 */
void gvizArrayMove(gvizArray *dest, gvizArray *src);

/**
 * Writes the data of a gvizArray to a stream.
 *
 * The output is formatted like so:
 *   [ item1, item2, ..., itemN, ]
 *
 * @param v      A pointer to the gvizArray to be printed.
 * @param stream A pointer to the output stream.
 * @param serialize A pointer to a function that will serialize each element.
 * @param bufsize   The size with which to initialize the element serialize buf.
 */
void gvizArrayPrint(const gvizArray *v, FILE *stream,
                    gvizSerializeDatum *serialize, size_t bufsize);

/**
 * Frees the memory for the underlying array.
 *
 * @param v The gvizArray that owns the array to be be destroyed.
 */
void gvizArrayRelease(gvizArray *v);

/**
 * Returns a pointer to the ith element in @p v, based on elementSize.
 * This function does NOT check if i is in the bounds of v.
 *
 * @param v The array to access
 *
 * @return A pointer to the element at that index.
 */
void *gvizArrayAtIndex(const gvizArray *v, size_t i);

/**
 * Returns a pointer to the last element in the array. Does not check if the
 * array is empty.
 *
 * @param v The array to access
 *
 * @return A pointer to the last element.
 */
void *gvizArrayTail(const gvizArray *v);

#endif
