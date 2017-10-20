#include "memmgr/BlockAllocator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#ifndef ALIGN
#define ALIGN(x, a)         (((x) + ((a) - 1)) & ~((a) - 1))
#endif

//#define DUMP_INFO

namespace mm
{

BlockAllocator::BlockAllocator()
        : m_pPageList(nullptr), m_pFreeList(nullptr), 
        m_szDataSize(0), m_szPageSize(0), 
        m_szAlignmentSize(0), m_szBlockSize(0), m_nBlocksPerPage(0) 
{
}

BlockAllocator::BlockAllocator(size_t data_size, size_t page_size, size_t alignment)
        : m_pPageList(nullptr), m_pFreeList(nullptr)
{
    Reset(data_size, page_size, alignment);
}

BlockAllocator::~BlockAllocator()
{
    FreeAll();
}

void BlockAllocator::Reset(size_t data_size, size_t page_size, size_t alignment)
{
    FreeAll();

    m_szDataSize = data_size;
    m_szPageSize = page_size;

    size_t minimal_size = (sizeof(BlockHeader) > m_szDataSize) ? sizeof(BlockHeader) : m_szDataSize;
    // this magic only works when alignment is 2^n, which should general be the case
    // because most CPU/GPU also requires the aligment be in 2^n
    // but still we use a assert to guarantee it
#if defined(_DEBUG)
    assert(alignment > 0 && ((alignment & (alignment-1))) == 0);
#endif
    m_szBlockSize = ALIGN(minimal_size, alignment);

    m_szAlignmentSize = m_szBlockSize - minimal_size;

    m_nBlocksPerPage = (m_szPageSize - sizeof(PageHeader)) / m_szBlockSize;
}

#ifdef DUMP_INFO
static int TOT_COUNT = 0;
static int TOT_FREE_COUNT = 0;
static int TOT_FREE_SZ = 0;
#endif // DUMP_INFO

void* BlockAllocator::Allocate()
{
    if (!m_pFreeList) {
		assert(m_nFreeBlocks == 0);

#ifdef DUMP_INFO
		printf("mem new page sz %d, count %d£¬ free count %d   pages(8kb) %f\n", m_szBlockSize, TOT_COUNT++, TOT_FREE_COUNT, TOT_FREE_SZ / 8192.0f);
#endif // DUMP_INFO

        // allocate a new page
        PageHeader* pNewPage = reinterpret_cast<PageHeader*>(new uint8_t[m_szPageSize]);
        ++m_nPages;
        m_nBlocks += m_nBlocksPerPage;
        m_nFreeBlocks += m_nBlocksPerPage;

#ifdef DUMP_INFO
		TOT_FREE_COUNT += m_nBlocksPerPage;
		TOT_FREE_SZ += m_szBlockSize * m_nBlocksPerPage;
#endif // DUMP_INFO

#if defined(_DEBUG)
        FillFreePage(pNewPage);
#endif

        if (m_pPageList) {
            pNewPage->pNext = m_pPageList;
        }

        m_pPageList = pNewPage;

        BlockHeader* pBlock = pNewPage->Blocks();
        // link each block in the page
        for (uint32_t i = 0; i < m_nBlocksPerPage - 1; i++) {
            pBlock->pNext = NextBlock(pBlock);
            pBlock = NextBlock(pBlock);
        }
        pBlock->pNext = nullptr;

        m_pFreeList = pNewPage->Blocks();
    }

    BlockHeader* freeBlock = m_pFreeList;
    m_pFreeList = m_pFreeList->pNext;
    --m_nFreeBlocks;

#ifdef DUMP_INFO
	TOT_FREE_COUNT--;
	TOT_FREE_SZ -= m_szBlockSize;
#endif // DUMP_INFO

#if defined(_DEBUG)
    FillAllocatedBlock(freeBlock);
#endif

    return reinterpret_cast<void*>(freeBlock);
}

void BlockAllocator::Free(void* p)
{
    BlockHeader* block = reinterpret_cast<BlockHeader*>(p);

#if defined(_DEBUG)
    FillFreeBlock(block);
#endif

    block->pNext = m_pFreeList;
    m_pFreeList = block;
    ++m_nFreeBlocks;

#ifdef DUMP_INFO
	TOT_FREE_COUNT++;
	TOT_FREE_SZ += m_szBlockSize;
#endif // DUMP_INFO
}

void BlockAllocator::FreeAll()
{
    PageHeader* pPage = m_pPageList;
    while(pPage) {
        PageHeader* _p = pPage;
        pPage = pPage->pNext;

        delete[] reinterpret_cast<uint8_t*>(_p);
    }

    m_pPageList = nullptr;
    m_pFreeList = nullptr;

    m_nPages        = 0;
    m_nBlocks       = 0;
    m_nFreeBlocks   = 0;
}

#if defined(_DEBUG)
void BlockAllocator::FillFreePage(PageHeader *pPage)
{
    // page header
    pPage->pNext = nullptr;
 
    // blocks
    BlockHeader *pBlock = pPage->Blocks();
    for (uint32_t i = 0; i < m_nBlocksPerPage; i++)
    {
        FillFreeBlock(pBlock);
        pBlock = NextBlock(pBlock);
    }
}
 
void BlockAllocator::FillFreeBlock(BlockHeader *pBlock)
{
    // block header + data
    memset(pBlock, PATTERN_FREE, m_szBlockSize - m_szAlignmentSize);
 
    // alignment
    memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize, 
                PATTERN_ALIGN, m_szAlignmentSize);
}
 
void BlockAllocator::FillAllocatedBlock(BlockHeader *pBlock)
{
    // block header + data
    memset(pBlock, PATTERN_ALLOC, m_szBlockSize - m_szAlignmentSize);
 
    // alignment
    memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize, 
                PATTERN_ALIGN, m_szAlignmentSize);
}
 
#endif

BlockHeader* BlockAllocator::NextBlock(BlockHeader *pBlock)
{
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize);
}

}