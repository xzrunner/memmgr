#ifndef _MEMMGR_MEMORY_POOL_H_
#define _MEMMGR_MEMORY_POOL_H_

#include "memmgr/LinearAllocator.h"

namespace mm
{

class MemoryPool
{
public:
	static MemoryPool* Instance();

private:
	MemoryPool();
	~MemoryPool();
	
private:
	static MemoryPool* m_instance;

}; // MemoryPool

}

#endif // _MEMMGR_MEMORY_POOL_H_