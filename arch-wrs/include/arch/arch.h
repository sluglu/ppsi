#ifndef __ARCH_H__
#define __ARCH_H__
#include <ppsi/assert.h>

/* Architecture-specific defines, included by top-level stuff */

#include <arpa/inet.h> /* ntohs etc */
#include <stdlib.h>    /* abs */

extern const int endianess; /* use to check endianess */

#define htonll(x) ((*(char *)&endianess == 1) ? \
		htobe64(x)  /* Little endian */ \
		: \
		(x))        /* Big endian */

#define ntohll(x) ((*(char *)&endianess == 1) ? \
		be64toh(x) /* Little endian */ \
		: \
		(x))       /* Big endian */

#endif /* __ARCH_H__ */
