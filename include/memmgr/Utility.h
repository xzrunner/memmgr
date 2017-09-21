#ifndef _MEMMGR_UTILITY_H_
#define _MEMMGR_UTILITY_H_

#include <stddef.h>

namespace mm
{

class Utility
{
public:
	static const char* ToSize(size_t value, float& result);

}; // Utility

}

#endif // _MEMMGR_UTILITY_H_