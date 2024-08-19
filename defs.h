#ifndef DEFS_H_
#define DEFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#define lengthof(arr) (sizeof(arr) / sizeof(*arr))
#define containerof(ptr, contype, membpath) ((contype*)(0 ? (void)(((contype*)NULL)->membpath = *(ptr)) : (void)0, ((char *)(ptr)) - offsetof(contype, membpath)))
// Assuming child type has a field for the base type
// So for structs it is usually actual downcast, but for unions it is an upcast
#define DOWNCAST(contype, basename, ptr) containerof(ptr, contype, as_##basename)
// Expects ptr to be of type srctype* or void*, returns (dsttype*)ptr
#define IMPLICIT_CAST(dsttype, srctype, ptr) (((union { typeof(srctype) *src; typeof(dsttype) *dst; }){.src = (ptr)}).dst)
#define T_ALLOC(count, T) ((T*)calloc((count), sizeof(T)))
#define T_REALLOC(ptr, count, T) ((T*)reallocarray(IMPLICIT_CAST(void, T, ptr), (count), sizeof(T)))

#define MODULE_CONSTRUCTOR(name) __attribute__((constructor)) static void name(void)

#define DEBUG_PRINT_VALUE(x, fmt) fprintf(stderr, #x " = " fmt "\n", x); fflush(stderr)

#endif /* end of include guard: DEFS_H_ */
