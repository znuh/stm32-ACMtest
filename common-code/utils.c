#include "utils.h"

/* val:          value
 * buf:          dst buffer
 * n:            number of digits plus decimal point if applicable
 * point_ofs:    decimal point offset (-1 for none) relative from least significant digit
 * zeropad:      prepend leading zeroes all the way
 * 
 * returns:      pointer to first digit
 */
char *i32_to_dec(int32_t val, char *buf, unsigned int n, int point_ofs, unsigned int zeropad) {
	unsigned int i, neg = val < 0;
	char *res;
	val = neg ? -val : val;
	buf += n - 1;
	buf[1] = 0;
	res = buf;
	for(i=0;i<n;i++,buf--) {
		char c = '0';
		if((int)i == point_ofs)
			c = '.';
		else if(val) {
			uint32_t new = val/10;
			c = val - (new * 10) + '0';		
			val = new;
		}
		else if(((int)i > (point_ofs+1)) && (!zeropad))
			c = ' ';
		*buf = c;
		res = (c == ' ') ? res : buf;
	}
	if(neg)
		*(--res)='-';
	return res;
}

static inline char nibble2hex(uint8_t v) {
	char nibble = v & 0xf;
	return nibble + ((nibble > 9) ? ('a' - 10) : '0');
}

void u32_to_hex(uint32_t val, char *dst) {
	val = __builtin_bswap32(val);
	for (int i = 4; i ; i--, val >>= 8) {
		*dst++ = nibble2hex(val>>4);
		*dst++ = nibble2hex(val);
	}
	*dst = 0;
}
