#ifndef BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP

#include <type_traits>
#include <memory>
#include <cstddef> // ptrdiff_t
#include <cstring> // memcpy
#include <type_traits>
#include <limits>

#include <boost/assert.hpp>
#include <boost/aligned_storage.hpp>

#include <boost/container/vector.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/container/detail/iterators.hpp>

namespace boost {
namespace container {

template <unsigned FrontSize, unsigned BackSize>
struct devector_small_buffer_policy
{
  BOOST_STATIC_CONSTANT(unsigned, front_size = FrontSize);
  BOOST_STATIC_CONSTANT(unsigned, back_size = BackSize);
  BOOST_STATIC_CONSTANT(unsigned, size = front_size + back_size);
};

/**
 * N: number of objects in the small buffer
 */
template <unsigned N>
struct devector_store_n : public devector_small_buffer_policy<0, N> {};

struct devector_default_growth_policy
{
  template <class SizeType>
  static SizeType new_capacity(SizeType old_capacity)
  {
    // @remark: we grow the capacity quite aggressively.
    //          this is justified since we aim to minimize
    //          heap-allocations, and because we mostly use
    //          the buffer locally.
    return (old_capacity) ? old_capacity * 4u : 10;
  }
};

struct reserve_only_tag {};

template <
  typename T,
  typename Allocator = std::allocator<T>,
  typename SmallBufferPolicy = devector_small_buffer_policy<0,0>,
  typename GrowthPolicy = devector_default_growth_policy
>
class devector : Allocator
{
  // Deallocate buffer on exception
  typedef typename container_detail::vector_value_traits<Allocator>::ArrayDeallocator allocation_guard;

  // Destroy already constructed elements
  typedef typename container_detail::vector_value_traits<Allocator>::ArrayDestructor construction_guard;

  typedef constant_iterator<T, int> cvalue_iterator;

// Standard Interface
public:
  // types:
  typedef T value_type;
  typedef Allocator allocator_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef typename std::allocator_traits<Allocator>::pointer pointer;
  typedef typename std::allocator_traits<Allocator>::const_pointer const_pointer;
  typedef pointer iterator;
  typedef const_pointer const_iterator;
  typedef unsigned int size_type;
  typedef int difference_type;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  // construct/copy/destroy
  devector() noexcept
    :_buffer(_storage.small_buffer_address()),
     _front_index(SmallBufferPolicy::front_size),
     _back_index(SmallBufferPolicy::front_size)
  {}

  explicit devector(const Allocator& allocator) noexcept
    :Allocator(allocator),
     _buffer(_storage.small_buffer_address()),
     _front_index(SmallBufferPolicy::front_size),
     _back_index(SmallBufferPolicy::front_size)
  {}

  // TODO this reserves back only
  explicit devector(size_type n, reserve_only_tag, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _storage(n),
     _buffer(allocate(_storage._capacity)),
     _front_index(),
     _back_index()
  {}

  explicit devector(size_type n, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _storage(n),
     _buffer(allocate(_storage._capacity)),
     _front_index(),
     _back_index(n)
  {
    // Cannot use construct_from_range/constant_iterator and copy_range,
    // because we are not allowed to default construct T

    allocation_guard buffer_guard(_buffer, get_allocator_ref(), _storage._capacity);
    if (is_small()) { buffer_guard.release(); } // avoid disposing small buffer

    construction_guard copy_guard(_buffer, get_allocator_ref(), 0u);

    for (size_type i = 0; i < n; ++i)
    {
      // TODO before C++11:
      //Allocator::construct(_buffer + i, T());
      std::allocator_traits<Allocator>::construct(get_allocator_ref(), _buffer + i);

      copy_guard.increment_size(1u);

      #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
        ++elem_copy_count;
      #endif
    }

    copy_guard.release();
    buffer_guard.release();
  }

  devector(size_type n, const T& value, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _storage(n),
     _buffer(allocate(_storage._capacity)),
     _front_index(),
     _back_index(n)
  {
    construct_from_range(cvalue_iterator(value, n), cvalue_iterator());
  }

  template <class InputIterator>
  devector(InputIterator first, InputIterator last, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _storage(std::distance(first, last)),
     _buffer(allocate(_storage._capacity)),
     _front_index(),
     _back_index(std::distance(first, last))
  {
    construct_from_range(first, last);
  }

  devector(const devector& x)
    :devector(x.begin(), x.end())
  {}

  devector(const devector& x, const Allocator& allocator)
    :devector(x.begin(), x.end(), allocator)
  {}

  template <class U, class A, class SBP, class GP>
  devector(const devector<U, A, SBP, GP>& x, const Allocator& allocator = Allocator())
    :devector(x.begin(), x.end(), allocator)
  {}

  devector(devector&&) noexcept;
  devector(devector&&, const Allocator& allocator);

  template <class U, class A, class SBP, class GP>
  devector(devector<U, A, SBP, GP>&&, const Allocator& allocator = Allocator());

  devector(const std::initializer_list<T>& range, const Allocator& allocator = Allocator())
    :devector(range.begin(), range.end(), allocator)
  {}

  ~devector()
  {
    destroy_elements(_buffer + _front_index, _buffer + _back_index);
    deallocate_buffer();
  }

  devector& operator=(const devector& x);
  devector& operator=(devector&& x) noexcept(
    std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
    std::allocator_traits<Allocator>::is_always_equal::value
  );

  template <class U, class A, class SBP, class GP>
  devector& operator=(const devector<U, A, SBP, GP>& x);

  template <class U, class A, class SBP, class GP>
  devector& operator=(devector<U, A, SBP, GP>&& x); /* noexcept? */

  devector& operator=(std::initializer_list<T>);

  template <class InputIterator>
  void assign(InputIterator first, InputIterator last);
  void assign(size_type n, const T& u);
  void assign(std::initializer_list<T>);

  allocator_type get_allocator() const noexcept
  {
    return static_cast<const Allocator&>(*this);
  }

  // iterators:
  iterator begin() noexcept
  {
    return _buffer + _front_index;
  }

  const_iterator begin() const noexcept
  {
    return _buffer + _front_index;
  }

  iterator end() noexcept
  {
    return _buffer + _back_index;
  }

  const_iterator end() const noexcept
  {
    return _buffer + _back_index;
  }

  reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(_buffer + _back_index);
  }

  const_reverse_iterator rbegin() const noexcept
  {
    return const_reverse_iterator(_buffer + _back_index);
  }

  reverse_iterator rend() noexcept
  {
    return reverse_iterator(_buffer + _front_index);
  }

  const_reverse_iterator rend() const noexcept
  {
    return const_reverse_iterator(_buffer + _front_index);
  }

  const_iterator cbegin() const noexcept
  {
    return _buffer + _front_index;
  }

  const_iterator cend() const noexcept
  {
    return _buffer + _back_index;
  }

  const_reverse_iterator crbegin() const noexcept
  {
    return const_reverse_iterator(_buffer + _back_index);
  }

  const_reverse_iterator crend() const noexcept
  {
    return const_reverse_iterator(_buffer + _front_index);
  }

  // capacity
  bool empty() const noexcept
  {
    return _front_index == _back_index;
  }

  size_type size() const noexcept
  {
    return _back_index - _front_index;
  }

  size_type max_size() const noexcept
  {
    return std::numeric_limits<size_type>::max();
  }

  size_type capacity() const noexcept
  {
    return _storage._capacity;
  }

  size_type front_free_capacity() const noexcept
  {
    return _front_index;
  }

  size_type back_free_capacity() const noexcept
  {
    return _storage._capacity - _back_index;
  }

  void resize(size_type sz) { resize_back(sz); }
  void resize(size_type sz, const T& c) { resize_back(sz, c); }

  void resize_front(size_type sz);
  void resize_front(size_type sz, const T& c);

  void resize_back(size_type sz)
  {
    if (sz > size())
    {
      if (sz > back_capacity())
      {
        reallocate_at(sz + _front_index, _front_index);
      }

      size_type n = sz - size();
      construct_n(_buffer + _back_index, n);
      _back_index += n;
    }
    else
    {
      while (size() > sz)
      {
        pop_back();
      }
    }

    BOOST_ASSERT(invariants_ok());
  }

  void resize_back(size_type sz, const T& c)
  {
    if (sz > size())
    {
      if (sz > back_capacity())
      {
        reallocate_at(sz + _front_index, _front_index);
      }

      size_type n = sz - size();
      construct_n(_buffer + _back_index, n, c);
      _back_index += n;
    }
    else
    {
      while (size() > sz)
      {
        pop_back();
      }
    }

    BOOST_ASSERT(invariants_ok());
  }

  // reserve promise:
  // after reserve_[front,back](n), n - size() push_[front,back] will not allocate

  void reserve(size_type new_capacity) { reserve_back(new_capacity); }

  void reserve_front(size_type new_capacity)
  {
    if (front_capacity() >= new_capacity) { return; }

    reallocate_at(new_capacity + back_capacity(), _front_index + new_capacity);

    BOOST_ASSERT(invariants_ok());
  }

  void reserve_back(size_type new_capacity)
  {
    if (back_capacity() >= new_capacity) { return; }

    reallocate_at(new_capacity + front_capacity(), _front_index);

    BOOST_ASSERT(invariants_ok());
  }

  void shrink_to_fit();

  // element access:
  reference operator[](size_type n)
  {
    return *(begin() + n);
  }

  const_reference operator[](size_type n) const
  {
    return *(begin() + n);
  }

  const_reference at(size_type n) const
  {
    if (size() <= n) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  reference at(size_type n)
  {
    if (size() <= n) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  reference front()
  {
    return (*this)[0];
  }

  const_reference front() const
  {
    return (*this)[0];
  }

  reference back()
  {
    return (*this)[size() - 1];
  }

  const_reference back() const
  {
    return (*this)[size() - 1];
  }

  // data access
  T* data() noexcept
  {
    return _buffer + _front_index;
  }

  const T* data() const noexcept
  {
    return _buffer + _front_index;
  }

  // modifiers:

  template <class... Args>
  void emplace_front(Args&&... args);

  void push_front(const T& x)
  {
    if (_front_index <= 0)
    {
      size_type new_capacity = calculate_new_capacity(_storage._capacity + 1);
      size_type start = new_capacity - _storage._capacity;
      reallocate_at(new_capacity, start);
    }

    --_front_index;
    std::allocator_traits<Allocator>::construct(get_allocator_ref(), _buffer + _front_index, x);

    BOOST_ASSERT(invariants_ok());
  }

//  void push_front(T&& x);

  void pop_front()
  {
    std::allocator_traits<Allocator>::destroy(get_allocator_ref(), _buffer + _front_index);
    ++_front_index;
    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  void emplace_back(Args&&... args);

  void push_back(const T& x)
  {
    if (_back_index >= _storage._capacity)
    {
      size_type new_capacity = calculate_new_capacity(_storage._capacity + 1);
      reallocate_at(new_capacity, _front_index);
    }

    std::allocator_traits<Allocator>::construct(get_allocator_ref(), _buffer + _back_index, x);
    ++_back_index;

    BOOST_ASSERT(invariants_ok());
  }

//  void push_back(T&& x);

  void pop_back()
  {
    --_back_index;
    std::allocator_traits<Allocator>::destroy(get_allocator_ref(), _buffer + _back_index);
    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args> iterator
  emplace(const_iterator position, Args&&... args);

  iterator insert(const_iterator position, const T& x);
  iterator insert(const_iterator position, T&& x);
  iterator insert(const_iterator position, size_type n, const T& x);

  template <class InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last);
  iterator insert(const_iterator position, std::initializer_list<T> il);

  iterator erase(const_iterator position);
  iterator erase(const_iterator first, const_iterator last);

  void swap(devector&) noexcept(
    std::allocator_traits<Allocator>::propagate_on_container_swap::value ||
    std::allocator_traits<Allocator>::is_always_equal::value);

  void clear() noexcept;

private:

  // Allocator wrappers

  allocator_type& get_allocator_ref() noexcept
  {
    return static_cast<Allocator&>(*this);
  }

  pointer allocate(size_type capacity)
  {
    if (capacity <= storage_t::small_buffer_size)
    {
      return _storage.small_buffer_address();
    }
    else
    {
      #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
      ++capacity_alloc_count;
      #endif // BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
      return std::allocator_traits<Allocator>::allocate(get_allocator_ref(), capacity);
    }
  }

  void destroy_elements(pointer begin, pointer end)
  {
    for (; begin != end; ++begin)
    {
      std::allocator_traits<Allocator>::destroy(get_allocator_ref(), begin);
    }
  }

  void deallocate_buffer()
  {
    if (! is_small() && _buffer)
    {
      std::allocator_traits<Allocator>::deallocate(get_allocator_ref(), _buffer, _storage._capacity);
    }
  }

  size_type front_capacity()
  {
    return _back_index;
  }

  size_type back_capacity()
  {
    return _storage._capacity - _front_index;
  }

  size_type calculate_new_capacity(size_type requested_capacity)
  {
    size_type policy_capacity = GrowthPolicy::new_capacity(_storage._capacity);
    return (std::max)(requested_capacity, policy_capacity);
  }

  void move_or_copy(pointer dst, pointer begin, pointer end)
  {
    for (; begin != end; ++begin, ++dst)
    {
      std::allocator_traits<Allocator>::construct(
        get_allocator_ref(),
        dst,
        std::move_if_noexcept(*begin)
      );
    }
  }

  void guarded_move_or_copy(pointer dst, pointer begin, pointer end)
  {
    construction_guard copy_guard(dst, get_allocator_ref(), 0u);

    for (; begin != end; ++begin, ++dst)
    {
      std::allocator_traits<Allocator>::construct(
        get_allocator_ref(),
        dst,
        std::move_if_noexcept(*begin)
      );

      copy_guard.increment_size(1u);
    }

    copy_guard.release();
  }

  void reallocate_at(size_type new_capacity, size_type buffer_offset)
  {
    BOOST_ASSERT(new_capacity > storage_t::small_buffer_size);
    pointer new_buffer = allocate(new_capacity);

    // TODO use is_trivially_copyable instead of is_pod
    if (std::is_pod<T>::value)
    {
      memcpy(
        new_buffer + buffer_offset,
        _buffer + _front_index,
        size() * sizeof(T)
      );
    }
    else if (
       std::is_nothrow_move_constructible<T>::value
    || std::is_nothrow_copy_constructible<T>::value
    )
    {
      move_or_copy(new_buffer, _buffer, _buffer + size());
    }
    else
    {
      allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

      guarded_move_or_copy(new_buffer, _buffer, _buffer + size());

      new_buffer_guard.release();
    }

    #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
      elem_copy_count += size();
    #endif

    destroy_elements(_buffer + _front_index, _buffer + _back_index);
    deallocate_buffer();

    _buffer = new_buffer;
    _storage._capacity = new_capacity;

    _back_index = _back_index - _front_index + buffer_offset;
    _front_index = buffer_offset;

    BOOST_ASSERT(invariants_ok());
  }

  template <typename Iterator>
  void construct_from_range(Iterator begin, Iterator end)
  {
    allocation_guard buffer_guard(_buffer, get_allocator_ref(), _storage._capacity);
    if (is_small()) { buffer_guard.release(); } // avoid disposing small buffer

    copy_range(begin, end, _buffer);

    buffer_guard.release();
  }

  template <typename Iterator>
  void copy_range(Iterator begin, Iterator end, pointer dest)
  {
    construction_guard copy_guard(dest, get_allocator_ref(), 0u);

    for (; begin != end; ++begin, ++dest)
    {
      std::allocator_traits<Allocator>::construct(get_allocator_ref(), dest, *begin);

      copy_guard.increment_size(1u);

      #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
        ++elem_copy_count;
      #endif
    }

    copy_guard.release();
  }

  template <typename... Args>
  void construct_n(pointer buffer, size_type n, Args... args)
  {
    construction_guard ctr_guard(buffer, get_allocator_ref(), 0u);

    for (size_type i = 0; i < n; ++i)
    {
      std::allocator_traits<Allocator>::construct(
        get_allocator_ref(),
        buffer + i,
        std::forward<Args>(args)...
      );

      ctr_guard.increment_size(1u);
    }

    ctr_guard.release();
  }

  bool invariants_ok()
  {
    return
       (!_storage._capacity || _buffer)
    && _front_index <= _back_index
    && _back_index <= _storage._capacity
    && storage_t::small_buffer_size <= _storage._capacity;
  }

  // Small buffer

  bool is_small() const
  {
    return _storage._capacity <= storage_t::small_buffer_size;
  }

  typedef boost::aligned_storage<
    sizeof(T) * SmallBufferPolicy::size,
    std::alignment_of<T>::value
  > small_buffer_t;

  // Achieve optimal space by leveraging EBO
  struct storage_t : small_buffer_t
  {
    BOOST_STATIC_CONSTANT(size_type, small_buffer_size = SmallBufferPolicy::size);

    storage_t() : _capacity(small_buffer_size) {}
    storage_t(size_type capacity)
      : _capacity((std::max)(capacity, size_type{small_buffer_size}))
    {}

    T* small_buffer_address()
    {
      return static_cast<T*>(small_buffer_t::address());
    }

    const T* small_buffer_address() const
    {
      return static_cast<const T*>(small_buffer_t::address());
    }

    // The only reason _capacity is here to avoid wasting
    // space if `small_buffer_t` is empty.
    size_type _capacity;
  };

  storage_t _storage;
  // 4 bytes padding on 64 bit here
  T* _buffer;

  size_type _front_index;
  size_type _back_index;

#ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
public:
  size_type elem_copy_count = 0;
  size_type capacity_alloc_count = 0;

  void reset_alloc_stats()
  {
    elem_copy_count = 0;
    capacity_alloc_count = 0;
  }
#endif // BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
};

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator==(const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  if (x.size() != y.size()) { return false; }
  return std::equal(x.begin(), x.end(), y.begin());
}

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator< (const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  return std::lexicographical_compare( x.begin(), x.end(),
                                       y.begin(), y.end() );
}

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator!=(const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  return !(x == y);
}

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator> (const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  return (y < x);
}

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator>=(const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  return !(x < y);
}

template <class T, class AllocatorX, class SBPX, class GPX, class AllocatorY, class SBPY, class GPY>
bool operator<=(const devector<T, AllocatorX, SBPX, GPX>& x, const devector<T, AllocatorY, SBPY, GPY>& y)
{
  return !(y < x);
}

template <class T, class Allocator, class SBP, class GP>
void swap(devector<T, Allocator, SBP, GP>& x, devector<T, Allocator, SBP, GP>& y) noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

}} // namespace boost::container

#endif // BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
