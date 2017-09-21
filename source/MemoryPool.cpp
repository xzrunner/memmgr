#include "memmgr/MemoryPool.h"
#include "memmgr/FreelistAllocator.h"

namespace mm
{

MemoryPool* MemoryPool::m_instance = nullptr;

MemoryPool::MemoryPool()
{
	// 512 to 131072
	m_freelist_alloc = new FreelistAllocator(9, 17);
}

MemoryPool::~MemoryPool()
{
}

void MemoryPool::DumpMemoryStats() const
{
	m_freelist_alloc->DumpMemoryStats("freelist alloc ");
}

MemoryPool* MemoryPool::Instance()
{
	if (!m_instance) {
		m_instance = new MemoryPool();
	}
	return m_instance;
}

}