#ifndef _MEMMGR_FREELIST_ALLOCATOR_H_
#define _MEMMGR_FREELIST_ALLOCATOR_H_

namespace mm
{

class FreelistAllocator
{
public:
	FreelistAllocator(int min, int max);
	FreelistAllocator(const FreelistAllocator&) = delete;
	FreelistAllocator& operator = (const FreelistAllocator&) = delete;
	~FreelistAllocator();
	
	void* Allocate(size_t size);
	void  Free(void* p, size_t size);

private:
	int QueryPageIdx(size_t size) const;

private:
	class Page;

private:
	Page* m_pages;

	int m_min, m_max;

}; // FreelistAllocator

}

#endif // _MEMMGR_FREELIST_ALLOCATOR_H_