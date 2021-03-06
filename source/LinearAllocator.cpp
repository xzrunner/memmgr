/*
 * Copyright 2012, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_NDEBUG 1

#include "memmgr/LinearAllocator.h"
#include "memmgr/Utility.h"

#include <logger.h>

#include <stdlib.h>
#include <assert.h>

// The ideal size of a page allocation (these need to be multiples of 8)
#define INITIAL_PAGE_SIZE ((size_t)512) // 512b
#define MAX_PAGE_SIZE ((size_t)131072) // 128kb

// The maximum amount of wasted space we can have per page
// Allocations exceeding this will have their own dedicated page
// If this is too low, we will malloc too much
// Too high, and we may waste too much space
// Must be smaller than INITIAL_PAGE_SIZE
#define MAX_WASTE_RATIO (0.5f)

#if ALIGN_DOUBLE
#define ALIGN_SZ (sizeof(double))
#else
#define ALIGN_SZ (sizeof(int))
#endif

#define ALIGN(x) (((x) + ALIGN_SZ - 1 ) & ~(ALIGN_SZ - 1))
#define ALIGN_PTR(p) ((void*)(ALIGN((size_t)(p))))

#if LOG_NDEBUG
#define ADD_ALLOCATION()
#define RM_ALLOCATION()
#else
#include <utils/Thread.h>
#include <utils/Timers.h>
static size_t s_totalAllocations = 0;
static nsecs_t s_nextLog = 0;
static android::Mutex s_mutex;

static void _logUsageLocked() {
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    if (now > s_nextLog) {
        s_nextLog = now + milliseconds_to_nanoseconds(10);
        ALOGV("Total pages allocated: %zu", s_totalAllocations);
    }
}

static void _addAllocation(int count) {
    android::AutoMutex lock(s_mutex);
    s_totalAllocations += count;
    _logUsageLocked();
}

#define ADD_ALLOCATION(size) _addAllocation(1);
#define RM_ALLOCATION(size) _addAllocation(-1);
#endif

#define min(x,y) (((x) < (y)) ? (x) : (y))

namespace mm {

class LinearAllocator::Page {
public:
    Page* next() { return mNextPage; }
    void setNext(Page* next) { mNextPage = next; }

	Page(int pageSize = -1)
		: mPageSize(pageSize)
		, mNextPage(0)
	{}

    void* operator new(size_t /*size*/, void* buf) { return buf; }

    void* start() {
        return (void*) (((size_t)this) + sizeof(Page));
    }

    void* end(int pageSize) {
        return (void*) (((size_t)start()) + pageSize);
    }

	int GetPageSize() const { return mPageSize; }

private:
    Page(const Page& /*other*/) {}

	int mPageSize;

    Page* mNextPage;
};

LinearAllocator::LinearAllocator()
    : mPageSize(INITIAL_PAGE_SIZE)
    , mMaxAllocSize(static_cast<size_t>(INITIAL_PAGE_SIZE * MAX_WASTE_RATIO))
    , mNext(0)
    , mCurrentPage(0)
    , mPages(0)
    , mTotalAllocated(0)
    , mWastedSpace(0)
    , mPageCount(0)
    , mDedicatedPageCount(0) {}

LinearAllocator::LinearAllocator(FreelistAllocator* alloc)
	: mPageSize(INITIAL_PAGE_SIZE)
	, mMaxAllocSize(static_cast<size_t>(INITIAL_PAGE_SIZE * MAX_WASTE_RATIO))
	, mNext(0)
	, mCurrentPage(0)
	, mPages(0)
	, mTotalAllocated(0)
	, mWastedSpace(0)
	, mPageCount(0)
	, mDedicatedPageCount(0)
	, m_alloc(alloc)
{
}

LinearAllocator& LinearAllocator::operator = (const LinearAllocator& alloc)
{
	if (this == &alloc) {
		return *this;
	}

	this->~LinearAllocator();

	mPageSize     = alloc.mPageSize;
	mMaxAllocSize = alloc.mMaxAllocSize;
	mNext         = alloc.mNext;
	mCurrentPage  = alloc.mCurrentPage;
	mPages        = alloc.mPages;
	mDtorList     = alloc.mDtorList;

	m_alloc = alloc.m_alloc;

	mTotalAllocated     = alloc.mTotalAllocated;
	mWastedSpace        = alloc.mWastedSpace;
	mPageCount          = alloc.mPageCount;
	mDedicatedPageCount = alloc.mDedicatedPageCount;

	const_cast<LinearAllocator&>(alloc).mDtorList = nullptr;
	const_cast<LinearAllocator&>(alloc).mPages = nullptr;

	return *this;
}

LinearAllocator::~LinearAllocator(void) {
    while (mDtorList) {
        auto node = mDtorList;
        mDtorList = node->next;
        node->dtor(node->addr);
    }
    Page* p = mPages;
    while (p) {
        Page* next = p->next();
        p->~Page();

		int pageSize = p->GetPageSize();
		if (pageSize < 0) {
			free(p);
		} else {
			assert(m_alloc);
			m_alloc->Free(p, pageSize);
		}

        RM_ALLOCATION();
        p = next;
    }
}

void* LinearAllocator::start(Page* p) {
    return ALIGN_PTR((size_t)p + sizeof(Page));
}

void* LinearAllocator::end(Page* p) {
    return ((char*)p) + mPageSize;
}

bool LinearAllocator::fitsInCurrentPage(size_t size) {
    return mNext && ((char*)mNext + size) <= end(mCurrentPage);
}

void LinearAllocator::ensureNext(size_t size) {
    if (fitsInCurrentPage(size)) return;

    if (mCurrentPage && mPageSize < MAX_PAGE_SIZE) {
        mPageSize = min(MAX_PAGE_SIZE, mPageSize * 2);
        mMaxAllocSize = static_cast<size_t>(mPageSize * MAX_WASTE_RATIO);
        mPageSize = ALIGN(mPageSize);
    }
    mWastedSpace += mPageSize;
    Page* p = newPage(mPageSize);
    if (mCurrentPage) {
        mCurrentPage->setNext(p);
    }
    mCurrentPage = p;
    if (!mPages) {
        mPages = mCurrentPage;
    }
    mNext = start(mCurrentPage);
}

void* LinearAllocator::allocImpl(size_t size) {
    size = ALIGN(size);
    if (size > mMaxAllocSize && !fitsInCurrentPage(size)) {
        LOGI("Exceeded max size %zu > %zu", size, mMaxAllocSize);
        // Allocation is too large, create a dedicated page for the allocation
        Page* page = newPage(size);
        mDedicatedPageCount++;
        page->setNext(mPages);
        mPages = page;
        if (!mCurrentPage)
            mCurrentPage = mPages;
        return start(page);
    }
    ensureNext(size);
    void* ptr = mNext;
    mNext = ((char*)mNext) + size;
    mWastedSpace -= size;
    return ptr;
}

void LinearAllocator::addToDestructionList(Destructor dtor, void* addr) {
    static_assert(std::is_standard_layout<DestructorNode>::value,
                  "DestructorNode must have standard layout");
    static_assert(std::is_trivially_destructible<DestructorNode>::value,
                  "DestructorNode must be trivially destructable");
    auto node = new (allocImpl(sizeof(DestructorNode))) DestructorNode();
    node->dtor = dtor;
    node->addr = addr;
    node->next = mDtorList;
    mDtorList = node;
}

void LinearAllocator::runDestructorFor(void* addr) {
    auto node = mDtorList;
    DestructorNode* previous = nullptr;
    while (node) {
        if (node->addr == addr) {
            if (previous) {
                previous->next = node->next;
            } else {
                mDtorList = node->next;
            }
            node->dtor(node->addr);
            rewindIfLastAlloc(node, sizeof(DestructorNode));
            break;
        }
        previous = node;
        node = node->next;
    }
}

void LinearAllocator::rewindIfLastAlloc(void* ptr, size_t allocSize) {
    // First run the destructor as running the destructor will
    // also rewind for the DestructorNode allocation which will
    // have been allocated after this void* if it has a destructor
    runDestructorFor(ptr);
    // Don't bother rewinding across pages
    allocSize = ALIGN(allocSize);
    if (ptr >= start(mCurrentPage) && ptr < end(mCurrentPage)
            && ptr == ((char*)mNext - allocSize)) {
        mWastedSpace += allocSize;
        mNext = ptr;
    }
}

LinearAllocator::Page* LinearAllocator::newPage(size_t pageSize) {
    pageSize = ALIGN(pageSize + sizeof(LinearAllocator::Page));
    ADD_ALLOCATION();
    mTotalAllocated += pageSize;
    mPageCount++;
	void* buf = nullptr;
	if (m_alloc) {
		buf = m_alloc->Allocate(pageSize);
	}
	if (buf) {
		return new (buf) Page(pageSize);
	} else {
		buf = malloc(pageSize);
		return new (buf) Page();
	}
}

void LinearAllocator::dumpMemoryStats(const char* prefix) {
    float prettySize;
    const char* prettySuffix;
    prettySuffix = Utility::ToSize(mTotalAllocated, prettySize);
	LOGI("%sTotal allocated: %.2f%s", prefix, prettySize, prettySuffix);
    prettySuffix = Utility::ToSize(mWastedSpace, prettySize);
	LOGI("%sWasted space: %.2f%s (%.1f%%)", prefix, prettySize, prettySuffix,
          (float) mWastedSpace / (float) mTotalAllocated * 100.0f);
	LOGI("%sPages %zu (dedicated %zu)", prefix, mPageCount, mDedicatedPageCount);
}

}; // namespace mm
