#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _memmgr_wrap_c_h_
#define _memmgr_wrap_c_h_

#include <stddef.h>

void* mm_alloc(size_t size);
void  mm_free(void* p, size_t size);

#endif // _memmgr_wrap_c_h_

#ifdef __cplusplus
}
#endif