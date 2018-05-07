#ifndef __ARCH_H__
#define __ARCH_H__
#include <assert.h> /* wrpc-sw includes assert already */

/* This arch exports wr functions, so include this for consistency checking */
#include "../proto-ext-whiterabbit/wr-api.h"

/* Architecture-specific defines, included by top-level stuff */

#ifndef htons /* If we build as host process, we have them LE already */
#  define htons(x)      (x)
#  define htonl(x)      (x)

#  define ntohs htons
#  define ntohl htonl
#endif


/* Code optimization for WRPC architecture */
#ifndef CODEOPT_BMCA
	#define CODEOPT_BMCA        1 /* Code optimization for BMCA. Can be overwritten in the makefile*/
#endif

#define CODEOPT_ONE_PORT()               (1 && CODEOPT_BMCA==1)  /* Code optimization when only one port is used. */
#define CODEOPT_ROLE_MASTER_SLAVE_ONLY() (1 && CODEOPT_BMCA==1)  /* Code optimization when only one port is used. */


#define abs(x) ((x >= 0) ? x : -x)
#endif /* __ARCH_H__ */
