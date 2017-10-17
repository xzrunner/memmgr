#include "c_wrap_mm.h"

#include "memmgr/Allocator.h"

namespace mm
{

extern "C"
void* mm_alloc(size_t size)
{
	return AllocHelper::Allocate(size);
}

extern "C"
void  mm_free(void* p, size_t size)
{
	AllocHelper::Free(p, size);
}

}