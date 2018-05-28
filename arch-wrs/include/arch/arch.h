#ifndef __ARCH_H__
#define __ARCH_H__
#include <ppsi/assert.h>

/* This arch exports wr/l1e functions, so include this for consistency checking */
#include "../proto-ext-whiterabbit/wr-api.h"
#include "../proto-ext-l1sync/l1e-api.h"

/* Architecture-specific defines, included by top-level stuff */

#include <arpa/inet.h> /* ntohs etc */
#include <stdlib.h>    /* abs */

#endif /* __ARCH_H__ */
