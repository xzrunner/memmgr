#ifndef _MEMMGR_FREELIST_ALLOCATOR_H_
#define _MEMMGR_FREELIST_ALLOCATOR_H_

#include <stddef.h>

namespace mm
{

class FreelistAllocator
{
public:
	FreelistAllocator(size_t min_page_sz, size_t max_page_sz);
	FreelistAllocator(const FreelistAllocator&) = delete;
	FreelistAllocator& operator = (const FreelistAllocator&) = delete;
	~FreelistAllocator();
	
	void* Allocate(size_t size);
	void  Free(void* p, size_t size);

	void DumpMemoryStats(const char* prefix = "") const;

private:
	int QueryPageIdx(size_t size) const;

private:
	class Page;

private:
	Page* m_pages;

	size_t m_min_page_sz, m_max_page_sz;

	// Memory usage tracking
	size_t m_tot_allocated;
	size_t m_wasted_space;
	size_t m_page_count;

}; // FreelistAllocator

}

#endif // _MEMMGR_FREELIST_ALLOCATOR_H_