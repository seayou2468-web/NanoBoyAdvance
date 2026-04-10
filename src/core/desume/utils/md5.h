#ifndef DESMUME_UTILS_MD5_H
#define DESMUME_UTILS_MD5_H

#include <stddef.h>

#include "../types.h"

struct MD5DATA
{
	static const size_t size = 16;
	u8 data[size];
};

#endif
