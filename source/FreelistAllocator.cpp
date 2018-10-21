#include "memmgr/FreelistAllocator.h"
#include "memmgr/Utility.h"

#include <logger.h>

#include <cstdint>

#include <assert.h>
#include <math.h>

#include <stdio.h>

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

	void* Allocate(FreelistAllocator& alloc)
	{
		if (!m_freelist)
		{
			size_t sz = m_block_sz + sizeof(Block);
			Block* new_block = reinterpret_cast<Block*>(new uint8_t[sz]);
			if (!new_block) {
				return nullptr;
			}
			new_block->next = nullptr;
			new_block->data = reinterpret_cast<void*>(new_block + 1);
			m_freelist = new_block;

			alloc.m_tot_allocated += sz;
			alloc.m_page_count++;
		}
		else
		{
			alloc.m_wasted_space -= (m_block_sz + sizeof(Block));
		}
		Block* free = m_freelist;
		m_freelist = m_freelist->next;
		return free->data;
	}

	void Free(void* p, FreelistAllocator& alloc)
	{
		if (!p) {
			return;
		}
		Block* block = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(p) - sizeof(Block));
		block->next = m_freelist;
		m_freelist = block;
		alloc.m_wasted_space += (m_block_sz + sizeof(Block));
	}

	static const int HEADER_SIZE = 16;

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

FreelistAllocator::FreelistAllocator(size_t min_page_sz, size_t max_page_sz)
	: m_min_page_sz(min_page_sz)
	, m_max_page_sz(max_page_sz)
	, m_tot_allocated(0)
	, m_wasted_space(0)
	, m_page_count(0)
{
	assert(min_page_sz <= max_page_sz);
	m_pages = new Page[max_page_sz - min_page_sz + 1];

	size_t block_sz = static_cast<size_t>(pow(2, min_page_sz));
	for (int i = 0, n = max_page_sz - min_page_sz + 1; i < n; ++i, block_sz *= 2) {
		m_pages[i].Reset(block_sz + Page::HEADER_SIZE);
	}
}

FreelistAllocator::~FreelistAllocator()
{
	delete m_pages;
}

void* FreelistAllocator::Allocate(size_t size)
{
	int idx = QueryPageIdx(size);
	return idx < 0 ? nullptr : m_pages[idx].Allocate(*this);
}

void FreelistAllocator::Free(void* p, size_t size)
{
	int idx = QueryPageIdx(size);
	if (idx >= 0) {
		m_pages[idx].Free(p, *this);
	}
}

int FreelistAllocator::QueryPageIdx(size_t size) const
{
	assert(m_min_page_sz > 0 && m_min_page_sz <= m_max_page_sz);

	size -= Page::HEADER_SIZE;

	int idx = 0;
	size_t rval = 1;
	while (rval < size) {
		++idx;
		rval <<= 1;
	}

	if (static_cast<size_t>(idx) < m_min_page_sz) {
		idx = m_min_page_sz;
	}
	if (static_cast<size_t>(idx) > m_max_page_sz) {
		return -1;
	}

	return idx - m_min_page_sz;
}

void FreelistAllocator::DumpMemoryStats(const char* prefix) const
{
	float pretty_size;
	const char* pretty_suffix;
	pretty_suffix = Utility::ToSize(m_tot_allocated, pretty_size);
	LOGI("%sTotal allocated: %.2f%s", prefix, pretty_size, pretty_suffix);
	//pretty_suffix = Utility::ToSize(m_wasted_space, pretty_size);
	//LOGI("%sWasted space: %.2f%s (%.1f%%)", prefix, pretty_size, pretty_suffix,
	//	(float)m_wasted_space / (float)m_tot_allocated * 100.0f);
	//LOGI("%sPages %zu", prefix, m_page_count);
}

}