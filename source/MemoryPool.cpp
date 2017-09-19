#include "memmgr/MemoryPool.h"

namespace mm
{

MemoryPool* MemoryPool::m_instance = nullptr;

MemoryPool::MemoryPool()
{

}

MemoryPool::~MemoryPool()
{

}

MemoryPool* MemoryPool::Instance()
{
	if (!m_instance) {
		m_instance = new MemoryPool();
	}
	return m_instance;
}

}