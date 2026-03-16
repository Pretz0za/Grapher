#ifndef ULONGARRAY_H
#define ULONGARRAY_H

#ifndef ULA_ALLOC
#define ULA_ALLOC(x) malloc(x)
#endif

#ifndef ULA_REALLOC
#define ULA_REALLOC(x, y) realloc(x, y)
#endif

#ifndef ULA_DEALLOC
#define ULA_DEALLOC(x) free(x)
#endif

#include <stddef.h>
#include <stdio.h>

/**
 * @brief A dynamically allocated array of size_t's.
 *
 * This struct manages data for maintaining a dynamically sized and allocated
 * vector of unsigned longs (size_t). The memory pointed to by arr are owned
 * be the Vector, and are freed using destroyVec().
 */
typedef struct {
  size_t *arr;     /**< Underlying array of data */
  size_t count;    /**< Number of elements currently in arr */
  size_t capacity; /**< Maximum capacity of currently allocated arr */
} ULongArray;

/**
 * Allocates memory for an underlying array and initializes @p arr to a
 * ULongArray struct.
 *
 * @param arr A pointer to the memory to initialize. This memory is owned by the
 * caller, and should be freed by the caller.
 *
 * @return An erro code showing whether or not the operation was successful.
 * @retval 0  If the operation was successful.
 * @retval -1 If @p arr is NULL or if the memory allocation for the underlying
 * array fails. @p arr is still valid.
 */
int ulArrayInit(ULongArray *arr);
int ulArrayInitAtCapacity(ULongArray *arr, size_t initialCapacity);

/**
 * Pushes an item to the end of a ULongArr, reallocates if necessary.
 *
 * @param v a pointer to the ULongArr the item will be pushed to.
 * @param item the value of the item to be added to @p v.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the item was added succesfully.
 * @retval -1 If the reallocation fails. @p v is still valid.
 */
int ulArrayPush(ULongArray *v, size_t item);

/**
 * Deletes and returns the last element in a ULongArr.
 *
 * @param v   A pointer to the ULongArr the item will be deleted from.
 * @param res A pointer to memory to store the value of the popped element.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful. Result is stored in @p res.
 * @retval -1 If @p v is empty. @p res is not modified.
 */
int ulArrayPop(ULongArray *v, size_t *res);

/**
 * Deletes the first instance of an item from a ULongArr.
 *
 * @param v    A pointer to the ULongArr to delete from.
 * @param item The value of the item to delete.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval idx The index of the deleted item in @p v.
 * @retval -1  If the item was not found in the Vecotr.
 */
int ulArrayFindOneAndDelete(ULongArray *v, size_t item);

/**
 * Checks if a ULongArr is empty.
 *
 * @param v A pointer to the ULongArr to check.
 *
 * @return An integer signaling the result.
 * @retval 0 The ULongArr is not empty.
 * @retval 1 The ULongArr is empty.
 */
int ulArrayIsEmpty(ULongArray *v);

/**
 * Finds the first instance of an item in a ULongArr. O(n)
 *
 * @param v    A pointer to the ULongArr to search.
 * @param item The value of the item to search for.
 *
 * @return The index of the item in @p v. -1 if not found.
 * @retval idx The index of the item in @p v.
 * @retval -1  If the item was not found in the Vecotr.
 */
int ulArrayFindOne(ULongArray *v, size_t item);

/**
 * Copies the data of one ULongArr to another.
 *
 * @param dest A pointer to the ULongArr the data will be copied to.
 * @param src  A pointer to the ULongArr the data will be copied from.
 *
 * @return An integer showing whether or no the operation was successful.
 * @retval 0 If the data was copied successfully.
 * @retval -1 If the reallocation of memory failed. Both @p dest and @p src are
 *           still valid.
 */
int ulArrayCopy(ULongArray *dest, const ULongArray *src);

/**
 * Initalizes an address as an array copies the data of one ULongArr into it.
 *
 * @param dest A pointer to the ULongArr the data will be copied to.
 * @param src  A pointer to the ULongArr the data will be copied from.
 *
 * @return An integer showing whether or no the operation was successful.
 * @retval 0 If the data was copied successfully.
 * @retval -1 If the reallocation of memory failed. Both @p dest and @p src are
 *           still valid.
 */
int ulArrayClone(ULongArray *dest, const ULongArray *src);

/**
 * Moves the data of one ULongArr to another. After this operation, @p src is
 * invalid and @p dest has taken ownership of the memory that was previously
 * owned by it. The memory previously stored by @p dest is lost.
 *
 * @param dest A pointer to the ULongArr the data will be moved to.
 * @param src  A pointer to the ULongArr the data will be moved from.
 */
void ulArrayMove(ULongArray *dest, ULongArray *src);

/**
 * Writes the data of a ULongArr to a stream.
 *
 * The output is formatted like so:
 *   [ item1, item2, ..., itemN, ]
 *
 * @param v      A pointer to the ULongArr to be printed.
 * @param stream A pointer to the output stream.
 */
void ulArrayPrint(ULongArray *v, FILE *stream);

/**
 * Frees the memory for the underlying array.
 *
 * @param v The ULongArr that owns the array to be be destroyed.
 */
void ulArrayRelease(ULongArray *v);

#endif
