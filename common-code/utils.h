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
void u32_to_hex(uint32_t val, char *dst);

#endif /* UTILS_H */
