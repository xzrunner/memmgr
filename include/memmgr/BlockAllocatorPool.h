// git@github.com:netwarm007/GameEngineFromScratch.git

#ifndef _MEMMGR_BLOCK_ALLOCATOR_POOL_H_
#define _MEMMGR_BLOCK_ALLOCATOR_POOL_H_

#include "memmgr/BlockAllocator.h"

#include <cu/cu_macro.h>

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
private:
    static size_t*        m_pBlockSizeLookup;
    static BlockAllocator*     m_pAllocators;
private:
    static BlockAllocator* LookUpAllocator(size_t size);

	CU_SINGLETON_DECLARATION(BlockAllocatorPool);

}; // BlockAllocatorPool

}

#endif // _MEMMGR_BLOCK_ALLOCATOR_POOL_H_