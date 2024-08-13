#ifndef DEFS_H_
#define DEFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#define containerof(ptr, contype, membpath) ((contype*)(0 ? (void)(((contype*)NULL)->membpath = *(ptr)) : (void)0, ((char *)(ptr)) - offsetof(contype, membpath)))
// Assuming child type has a field for the base type
// So for structs it is usually actual downcast, but for unions it is an upcast
#define DOWNCAST(contype, basename, ptr) containerof(ptr, contype, as_##basename)

#define DEBUG_PRINT_VALUE(x, fmt) fprintf(stderr, #x " = " fmt "\n", x); fflush(stderr)

#endif /* end of include guard: DEFS_H_ */
