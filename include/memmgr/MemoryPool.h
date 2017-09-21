#ifndef _MEMMGR_MEMORY_POOL_H_
#define _MEMMGR_MEMORY_POOL_H_

namespace mm
{

class FreelistAllocator;

class MemoryPool
{
public:
	FreelistAllocator* GetFreelistAlloc() { return m_freelist_alloc; }

	void DumpMemoryStats() const;

	static MemoryPool* Instance();

private:
	MemoryPool();
	~MemoryPool();
	
private:
	FreelistAllocator* m_freelist_alloc;

	static MemoryPool* m_instance;

}; // MemoryPool

}

#endif // _MEMMGR_MEMORY_POOL_H_