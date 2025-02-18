#ifndef UTILS_H
#define UTILS_H

#ifndef MAX
#define MAX(a,b)    ((a)>=(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b)    ((a)<=(b)?(a):(b))
#endif

#include <stdint.h>
char *i32_to_dec(int32_t val, char *buf, unsigned int n, int point_ofs, unsigned int zeropad);

#endif /* UTILS_H */
