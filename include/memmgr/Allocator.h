#ifndef _MEMMGR_ALLOCATOR_H_
#define _MEMMGR_ALLOCATOR_H_

#include "memmgr/BlockAllocatorPool.h"

#include <memory>

#include <vector>
#include <string>

namespace mm
{

class AllocHelper
{
public:
	template<class T, typename... Arguments>
	static T* New(Arguments... parameters)
	{
		return BlockAllocatorPool::Instance()->New<T>(parameters...);
	}

	template<class T>
	static void Delete(T* p)
	{
		BlockAllocatorPool::Instance()->Delete<T>(p);
	}

	static void* Allocate(size_t size)
	{
		return BlockAllocatorPool::Instance()->Allocate(size);
	}

	static void Free(void* p, size_t size)
	{
		BlockAllocatorPool::Instance()->Free(p, size);
	}

}; // AllocHelper

template<typename T>
struct Allocator
{
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    template<typename U>
    struct rebind {typedef Allocator<U> other;};

    Allocator() throw() {};
    Allocator(const Allocator& other) throw() {};

    template<typename U>
    Allocator(const Allocator<U>& other) throw() {};

    template<typename U>
    Allocator& operator = (const Allocator<U>& other) { return *this; }
    Allocator<T>& operator = (const Allocator& other) { return *this; }
    ~Allocator() {}

    pointer allocate(size_type n, const void* hint = 0)
    {
        //return static_cast<T*>(::operator new(n * sizeof(T)));
		if (n != 1) {
			int zz = n * sizeof(T);
			int z = 0;
		}
		return static_cast<T*>(BlockAllocatorPool::Instance()->Allocate(n * sizeof(T)));
    }

    void deallocate(T* ptr, size_type n)
    {
		//::operator delete(ptr);
		if (n != 1) {
			int zz = n * sizeof(T);
			int z = 0;
		}
		BlockAllocatorPool::Instance()->Free(ptr, n * sizeof(T));
    }

}; // Allocator

template <typename T, typename U>
inline bool operator == (const Allocator<T>&, const Allocator<U>&)
{
    return true;
}

template <typename T, typename U>
inline bool operator != (const Allocator<T>& a, const Allocator<U>& b)
{
    return !(a == b);
}

template <typename T, typename ...Args>
std::shared_ptr<T> allocate_shared(Args&&... args) {
	return std::allocate_shared<T, Allocator<T>>(Allocator<T>(), std::forward<Args>(args)...);
}

template<typename Alloc>
struct alloc_deleter
{
	alloc_deleter(const Alloc& a) : a(a) { }

	typedef typename std::allocator_traits<Alloc>::pointer pointer;

	void operator()(pointer p) const
	{
		Alloc aa(a);
		std::allocator_traits<Alloc>::destroy(aa, std::addressof(*p));
		std::allocator_traits<Alloc>::deallocate(aa, p, 1);
	}

private:
	Alloc a;
};

template<typename T, typename Alloc, typename... Args>
auto _allocate_unique(const Alloc& alloc, Args&&... args)
{
	using AT = std::allocator_traits<Alloc>;
	static_assert(std::is_same<typename AT::value_type, std::remove_cv_t<T>>{}(),
		"Allocator has the wrong value_type");

	Alloc a(alloc);
	auto p = AT::allocate(a, 1);
	try {
		AT::construct(a, std::addressof(*p), std::forward<Args>(args)...);
		using D = alloc_deleter<Alloc>;
		return std::unique_ptr<T, D>(p, D(a));
	}
	catch (...)
	{
		AT::deallocate(a, p, 1);
		throw;
	}
}

template <typename T, typename ...Args>
auto allocate_unique(Args&&... args) {
	return _allocate_unique<T>(Allocator<T>(), std::forward<Args>(args)...);
}

template <typename T>
class AllocVector : public std::vector<T, Allocator<T>>
{
}; // AllocVector

using AllocString = std::basic_string<char, std::char_traits<char>, Allocator<char>>;

}

#endif // _MEMMGR_ALLOCATOR_H_