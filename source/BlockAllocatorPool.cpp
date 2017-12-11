#include "memmgr/BlockAllocatorPool.h"

//extern "C" void* malloc(size_t size);
//extern "C" void  free(void* p);
#include <stdlib.h>

#include <thread>

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
#else
#define CHECK_MT
#endif // __MINGW32__

#ifdef CHECK_MT
#include <assert.h>
#endif // CHECK_MT

#ifndef ALIGN
#define ALIGN(x, a)         (((x) + ((a) - 1)) & ~((a) - 1))
#endif

namespace mm 
{
static const uint32_t kBlockSizes[] = {
    // 4-increments
    4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48,
    52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 

    // 32-increments
    128, 160, 192, 224, 256, 288, 320, 352, 384, 
    416, 448, 480, 512, 544, 576, 608, 640, 

    // 64-increments
    704, 768, 832, 896, 960, 1024
};

static const uint32_t kPageSize  = 8192;
static const uint32_t kAlignment = 4;

// number of elements in the block size array
static const uint32_t kNumBlockSizes = 
    sizeof(kBlockSizes) / sizeof(kBlockSizes[0]);

// largest valid block size
static const uint32_t kMaxBlockSize = 
    kBlockSizes[kNumBlockSizes - 1];

thread_local size_t*             BlockAllocatorPool::m_pBlockSizeLookup;
thread_local BlockAllocator*     BlockAllocatorPool::m_pAllocators;
thread_local BlockAllocatorPool* BlockAllocatorPool::m_instance;

#ifdef CHECK_MT
thread_local static std::thread::id THIS_ID;
#endif // CHECK_MT

BlockAllocatorPool::BlockAllocatorPool()
{
	Initialize();
}

int BlockAllocatorPool::Initialize()
{
    // one-time initialization
    thread_local static bool s_bInitialized = false;
    if (!s_bInitialized) 
	{
        // initialize block size lookup table
        m_pBlockSizeLookup = new size_t[kMaxBlockSize + 1];
        size_t j = 0;
        for (size_t i = 0; i <= kMaxBlockSize; i++) {
            if (i > kBlockSizes[j]) ++j;
            m_pBlockSizeLookup[i] = j;
        }

        // initialize the allocators
        m_pAllocators = new BlockAllocator[kNumBlockSizes];
        for (size_t i = 0; i < kNumBlockSizes; i++) {
            m_pAllocators[i].Reset(kBlockSizes[i], kPageSize, kAlignment);
        }

		THIS_ID = std::this_thread::get_id();

        s_bInitialized = true;
    }

    return 0;
}

void BlockAllocatorPool::Finalize()
{
    delete[] m_pAllocators;
    delete[] m_pBlockSizeLookup;
}

void BlockAllocatorPool::Tick()
{
}

BlockAllocator* BlockAllocatorPool::LookUpAllocator(size_t size)
{
    // check eligibility for lookup
    if (size <= kMaxBlockSize)
        return m_pAllocators + m_pBlockSizeLookup[size];
    else
        return nullptr;
}

void* BlockAllocatorPool::Allocate(size_t size)
{
#ifdef CHECK_MT
	assert(std::this_thread::get_id() == THIS_ID);
#endif // CHECK_MT

	void* ret = nullptr;
    BlockAllocator* pAlloc = LookUpAllocator(size);
	if (pAlloc) {
		ret = pAlloc->Allocate();
	}
    else
        ret = malloc(size);

	return ret;
}

void* BlockAllocatorPool::Allocate(size_t size, size_t alignment)
{
#ifdef CHECK_MT
	assert(std::this_thread::get_id() == THIS_ID);
#endif // CHECK_MT

    uint8_t* p;
    size += alignment;
    BlockAllocator* pAlloc = LookUpAllocator(size);
    if (pAlloc)
        p = reinterpret_cast<uint8_t*>(pAlloc->Allocate());
    else
        p = reinterpret_cast<uint8_t*>(malloc(size));

    p = reinterpret_cast<uint8_t*>(ALIGN(reinterpret_cast<size_t>(p), alignment));
    
    return static_cast<void*>(p);
}

void BlockAllocatorPool::Free(void* p, size_t size)
{
#ifdef CHECK_MT
	assert(std::this_thread::get_id() == THIS_ID);
#endif // CHECK_MT

    BlockAllocator* pAlloc = LookUpAllocator(size);
    if (pAlloc)
        pAlloc->Free(p);
    else
        free(p);
}

BlockAllocatorPool* BlockAllocatorPool::Instance()
{
	if (!m_instance) {
		m_instance = new BlockAllocatorPool();
	}
	return m_instance;
}

}
