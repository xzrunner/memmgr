#include "memmgr/FreelistAllocator.h"

#include <cstdint>

#include <assert.h>

#define ALIGN(x) (((x) + ALIGN_SZ - 1 ) & ~(ALIGN_SZ - 1))
#define ALIGN_PTR(p) ((void*)(ALIGN((size_t)(p))))

namespace mm
{

class FreelistAllocator::Page
{
public:
	Page()
		: m_block_sz(0)
		, m_header(nullptr)
		, m_freelist(nullptr) 
	{}
	Page(const Page&) = delete;
	Page& operator = (const Page&) = delete;
	~Page() { FreeAll(); }

	void Reset(size_t block_sz)
	{
		if (m_block_sz != block_sz) {
			FreeAll();
			m_block_sz = block_sz;
		}
	}

	void* Allocate() 
	{
		if (!m_freelist)
		{
			Block* new_block = reinterpret_cast<Block*>(new uint8_t[m_block_sz + sizeof(Block)]);
			if (!new_block) {
				return nullptr;
			}
			new_block->next = nullptr;
			new_block->data = reinterpret_cast<void*>(new_block + 1);
			m_freelist = new_block;
		}
		Block* free = m_freelist;
		m_freelist = m_freelist->next;
		return free->data;
	}

	void Free(void* p)
	{
		if (!p) {
			return;
		}
		Block* block = reinterpret_cast<Block*>(p);
		block->next = m_freelist;
		m_freelist = block;
	}

private:
	void FreeAll()
	{
		Block* block = m_header;
		while (block) {
			Block* b = block;
			block = block->next;
			delete[] reinterpret_cast<uint8_t*>(b);
		}

		m_block_sz = 0;

		m_header = nullptr;
		m_freelist = nullptr;
	}
	
private:
	struct Block
	{
		Block* next;
		void* data;
	};

private:
	size_t m_block_sz;

	Block* m_header;
	Block* m_freelist;

}; // Page

FreelistAllocator::FreelistAllocator(int min, int max)
	: m_min(min)
	, m_max(max)
{
	assert(min <= max);
	m_pages = new Page[max - min + 1];
}

FreelistAllocator::~FreelistAllocator()
{
	delete m_pages;
}

void* FreelistAllocator::Allocate(size_t size)
{
	int idx = QueryPageIdx(size);
	return idx < 0 ? nullptr : m_pages[idx].Allocate();
}

void FreelistAllocator::Free(void* p, size_t size)
{
	int idx = QueryPageIdx(size);
	if (idx >= 0) {
		m_pages[idx].Free(p);
	}
}

int FreelistAllocator::QueryPageIdx(size_t size) const
{
	assert(m_min > 0 && m_min <= m_max);

	int idx = 0;
	int rval = 1;
	while (rval < size) {
		++idx;
		rval <<= 1;
	}

	if (idx < m_min) {
		idx = m_min;
	}
	if (idx > m_max) {
		return -1;
	}

	return idx - m_min;
}

}