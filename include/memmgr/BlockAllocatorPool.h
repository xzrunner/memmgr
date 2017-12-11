// git@github.com:netwarm007/GameEngineFromScratch.git

#ifndef _MEMMGR_BLOCK_ALLOCATOR_POOL_H_
#define _MEMMGR_BLOCK_ALLOCATOR_POOL_H_

#include "memmgr/BlockAllocator.h"

#include <new>

namespace mm 
{

class BlockAllocatorPool
{
public:
    template<class T, typename... Arguments>
    T* New(Arguments... parameters)
    {
        return new (Allocate(sizeof(T))) T(parameters...);
    }

    template<class T>
    void Delete(T* p)
    {
        p->~T();
        Free(p, sizeof(T));
    }

public:
    virtual int Initialize();
    virtual void Finalize();
    virtual void Tick();

    void* Allocate(size_t size);
    void* Allocate(size_t size, size_t alignment);
    void  Free(void* p, size_t size);

	static BlockAllocatorPool* Instance();

private:
	BlockAllocatorPool();
	~BlockAllocatorPool();

	static BlockAllocator* LookUpAllocator(size_t size);

private:
	thread_local static size_t*         m_pBlockSizeLookup;
	thread_local static BlockAllocator* m_pAllocators;

	thread_local static BlockAllocatorPool* m_instance;

}; // BlockAllocatorPool

}

#endif // _MEMMGR_BLOCK_ALLOCATOR_POOL_H_