#ifndef __BITSEARCH_H__
#define __BITSEARCH_H__

#include <libs/types.h>

static inline int bsf(uint32_t n) __attribute__((always_inline));
static inline int bsr(uint32_t n) __attribute__((always_inline));

static inline int
bsf(uint32_t n)
{
	if (n == 0) return -1;
	uint32_t result;
	__asm __volatile("bsfl %1, %0"
					 : "=r" (result)
					 : "r" (n));
	return result;
}

static inline int
bsr(uint32_t n)
{
	if (n == 0) return -1;
	uint32_t result;
	__asm __volatile("bsrl %1, %0"
					 : "=r" (result)
					 : "r" (n));
	return result;
}

#endif
