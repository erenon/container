#ifndef BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP

#include <type_traits>
#include <memory>
#include <cstddef> // ptrdiff_t
#include <cstring> // memcpy
#include <type_traits>
#include <limits>
#include <algorithm>

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

  template <class SizeType>
  static bool should_shrink(SizeType size, SizeType capacity, SizeType small_buffer_size)
  {
    (void)capacity;
    return size <= small_buffer_size;
  }
};

template <typename Allocator>
struct devector_allocator_traits
{
  static const bool is_trivially_copyable = false;
};

template <typename T>
struct devector_allocator_traits<std::allocator<T>>
{
  // TODO use is_trivially_copyable instead of is_pod
  static const bool is_trivially_copyable = std::is_pod<T>::value;
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
  // TODO make construction_guard less strict (enable null guard when possible)
  typedef typename container_detail::vector_value_traits<Allocator>::ArrayDestructor construction_guard;

  // When no guard needed
  typedef container_detail::null_scoped_destructor_n<Allocator> null_construction_guard;

  // Destroy either source or target
  typedef typename container_detail::nand_destroyer<Allocator> move_guard;

  typedef constant_iterator<T, int> cvalue_iterator;

  class allocator_traits : public std::allocator_traits<Allocator>,
                           public devector_allocator_traits<Allocator>
  {};

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
      alloc_construct(_buffer + i);
      copy_guard.increment_size(1u);
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

  // TODO do not iterate twice over input range

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

  // TODO use Allocator select_on_container_copy_construction in copy ctr
  // TODO take care of self assignment

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

  // TODO implement move constructors

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

  // TODO implement assignment

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

  void resize_front(size_type sz)
  {
    if (sz > size())
    {
      const size_type n = sz - size();

      if (sz < front_capacity())
      {
        construct_n(_buffer + _front_index - n, n);
        _front_index -= n;
      }
      else
      {
        resize_front_slow_path(sz, n);
      }
    }
    else
    {
      while (size() > sz)
      {
        pop_front();
      }
    }

    BOOST_ASSERT(invariants_ok());
  }

  void resize_front(size_type sz, const T& c)
  {
    if (sz > size())
    {
      const size_type n = sz - size();

      if (sz < front_capacity())
      {
        construct_n(_buffer + _front_index - n, n, c);
        _front_index -= n;
      }
      else
      {
        resize_front_slow_path(sz, n, c);
      }
    }
    else
    {
      while (size() > sz)
      {
        pop_front();
      }
    }

    BOOST_ASSERT(invariants_ok());
  }

  void resize_back(size_type sz)
  {
    if (sz > size())
    {
      const size_type n = sz - size();

      if (sz < back_capacity())
      {
        construct_n(_buffer + _back_index, n);
        _back_index += n;
      }
      else
      {
        resize_back_slow_path(sz, n);
      }
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
      const size_type n = sz - size();

      if (sz < back_capacity())
      {
        construct_n(_buffer + _back_index, n, c);
        _back_index += n;
      }
      else
      {
        resize_back_slow_path(sz, n, c);
      }
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

    reallocate_at(new_capacity + back_free_capacity(), new_capacity - size());

    BOOST_ASSERT(invariants_ok());
  }

  void reserve_back(size_type new_capacity)
  {
    if (back_capacity() >= new_capacity) { return; }

    reallocate_at(new_capacity + front_free_capacity(), _front_index);

    BOOST_ASSERT(invariants_ok());
  }

  void shrink_to_fit()
  {
    if (
       GrowthPolicy::should_shrink(size(), capacity(), storage_t::small_buffer_size) == false
    || is_small()
    )
    {
      return;
    }

    if (size() <= storage_t::small_buffer_size)
    {
      buffer_move_or_copy(_storage.small_buffer_address());

      _buffer = _storage.small_buffer_address();
      _storage._capacity = storage_t::small_buffer_size;
      _back_index = size();
      _front_index = 0;
    }
    else
    {
      reallocate_at(size(), 0);
    }
  }

  // element access:
  reference operator[](size_type n)
  {
    BOOST_ASSERT(!empty());

    return *(begin() + n);
  }

  const_reference operator[](size_type n) const
  {
    BOOST_ASSERT(!empty());

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
    BOOST_ASSERT(!empty());

    return *(_buffer + _front_index);
  }

  const_reference front() const
  {
    BOOST_ASSERT(!empty());

    return *(_buffer + _front_index);
  }

  reference back()
  {
    BOOST_ASSERT(!empty());

    return *(_buffer + _back_index -1);
  }

  const_reference back() const
  {
    BOOST_ASSERT(!empty());

    return *(_buffer + _back_index -1);
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
  void emplace_front(Args&&... args)
  {
    if (front_free_capacity()) // fast path
    {
      alloc_construct(_buffer + _front_index - 1, std::forward<Args>(args)...);
      --_front_index;
    }
    else
    {
      emplace_reallocating_slow_path(true, 0, std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  void push_front(const T& x)
  {
    emplace_front(x);
  }

  void push_front(T&& x)
  {
    emplace_front(std::move(x));
  }

  void unsafe_push_front(const T& x)
  {
    BOOST_ASSERT(front_free_capacity());

    alloc_construct(_buffer + _front_index - 1, x);
    --_front_index;

    BOOST_ASSERT(invariants_ok());
  }

  void unsafe_push_front(T&& x)
  {
    BOOST_ASSERT(front_free_capacity());

    alloc_construct(_buffer + _front_index - 1, std::forward<T>(x));
    --_front_index;

    BOOST_ASSERT(invariants_ok());
  }

  void pop_front()
  {
    BOOST_ASSERT(! empty());
    std::allocator_traits<Allocator>::destroy(get_allocator_ref(), _buffer + _front_index);
    ++_front_index;
    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  void emplace_back(Args&&... args)
  {
    if (back_free_capacity()) // fast path
    {
      alloc_construct(_buffer + _back_index, std::forward<Args>(args)...);
      ++_back_index;
    }
    else
    {
      emplace_reallocating_slow_path(false, size(), std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  void push_back(const T& x)
  {
    emplace_back(x);
  }

  void push_back(T&& x)
  {
    emplace_back(std::move(x));
  }

  void unsafe_push_back(const T& x)
  {
    BOOST_ASSERT(back_free_capacity());

    alloc_construct(_buffer + _back_index, x);
    ++_back_index;

    BOOST_ASSERT(invariants_ok());
  }

  void unsafe_push_back(T&& x)
  {
    BOOST_ASSERT(back_free_capacity());

    alloc_construct(_buffer + _back_index, std::forward<T>(x));
    ++_back_index;

    BOOST_ASSERT(invariants_ok());
  }

  void pop_back()
  {
    BOOST_ASSERT(! empty());
    --_back_index;
    std::allocator_traits<Allocator>::destroy(get_allocator_ref(), _buffer + _back_index);
    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  iterator emplace(const_iterator position, Args&&... args)
  {
    BOOST_ASSERT(position >= begin());
    BOOST_ASSERT(position <= end());

    if (position == end() && back_free_capacity()) // fast path
    {
      alloc_construct(_buffer + _back_index, std::forward<Args>(args)...);
      ++_back_index;
      return end() - 1;
    }
    else if (position == begin() && front_free_capacity()) // secondary fast path
    {
      alloc_construct(_buffer + _front_index - 1, std::forward<Args>(args)...);
      --_front_index;
      return begin();
    }
    else
    {
      size_type new_elem_index = position - begin();
      return emplace_slow_path(new_elem_index, std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  iterator insert(const_iterator position, const T& x)
  {
    return emplace(position, x);
  }

  iterator insert(const_iterator position, T&& x)
  {
    return emplace(position, std::move(x));
  }

  iterator insert(const_iterator position, size_type n, const T& x)
  {
    cvalue_iterator first(x, n);
    cvalue_iterator last = first + n;
    return insert_range(position, first, last);
  }

  // TODO add bidirectional iterator optimization

  template <class InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last)
  {
    devector range(first, last);
    return insert_range(position, range.begin(), range.end());
  }

  iterator insert(const_iterator position, std::initializer_list<T> il)
  {
    return insert_range(position, il.begin(), il.end());
  }

  iterator erase(const_iterator position)
  {
    return erase(position, position + 1);
  }

  iterator erase(const_iterator first, const_iterator last)
  {
    iterator nc_first = begin() + (first - begin());
    iterator nc_last  = begin() + (last  - begin());
    return erase(nc_first, nc_last);
  }

  iterator erase(iterator first, iterator last)
  {
    size_type front_distance = last - begin();
    size_type back_distance = end() - first;
    size_type n = std::distance(first, last);

    if (front_distance <= back_distance)
    {
      // rotate to front and destroy
      std::rotate(begin(), first, last);

      for (iterator i = begin(); i != begin() + n; ++i)
      {
        std::allocator_traits<Allocator>::destroy(get_allocator_ref(), i);
      }
      _front_index += n;

      BOOST_ASSERT(invariants_ok());
      return last;
    }
    else
    {
      // rotate to back and destroy
      std::rotate(first, last, end());

      for (iterator i = end() - n; i != end(); ++i)
      {
        std::allocator_traits<Allocator>::destroy(get_allocator_ref(), i);
      }
      _back_index -= n;

      BOOST_ASSERT(invariants_ok());
      return first;
    }
  }

  void swap(devector& b) noexcept
    (std::is_nothrow_copy_constructible<T>::value || std::is_nothrow_move_constructible<T>::value)
    // && nothrow_swappable
  {
    BOOST_ASSERT(
       ! std::allocator_traits<Allocator>::propagate_on_container_swap::value
    || get_allocator_ref() == b.get_allocator_ref()
    ); // else it's undefined behavior

    if (is_small())
    {
      if (b.is_small())
      {
        swap_small_small(*this, b);
      }
      else
      {
        swap_small_big(*this, b);
      }
    }
    else
    {
      if (b.is_small())
      {
        swap_small_big(b, *this);
      }
      else
      {
        swap_big_big(*this, b);
      }
    }

    BOOST_ASSERT(  invariants_ok());
    BOOST_ASSERT(b.invariants_ok());
  }

  void clear() noexcept
  {
    destroy_elements(begin(), end());
    _front_index = _back_index = SmallBufferPolicy::front_size;
  }

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

  template <typename... Args>
  void alloc_construct(pointer dst, Args&&... args)
  {
    std::allocator_traits<Allocator>::construct(
      get_allocator_ref(),
      dst,
      std::forward<Args>(args)...
    );
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
    // TODO check for max_size
  }

  void move_or_copy(pointer dst, pointer begin, const const_pointer end)
  {
    for (; begin != end; ++begin, ++dst)
    {
      alloc_construct(dst, std::move_if_noexcept(*begin));
    }
  }

  void guarded_move_or_copy(pointer dst, pointer begin, const const_pointer end)
  {
    construction_guard copy_guard(dst, get_allocator_ref(), 0u);

    guarded_move_or_copy(dst, begin, end, copy_guard);

    copy_guard.release();
  }

  template <typename Guard>
  void guarded_move_or_copy(pointer dst, pointer begin, const const_pointer end, Guard& guard)
  {
    for (; begin != end; ++begin, ++dst)
    {
      alloc_construct(dst, std::move_if_noexcept(*begin));
      guard.increment_size(1u);
    }
  }

  void buffer_move_or_copy(pointer dst)
  {
    construction_guard guard(dst, get_allocator_ref(), 0);

    buffer_move_or_copy(dst, guard);

    guard.release();
  }

  void buffer_move_or_copy(pointer dst, construction_guard& guard)
  {
    range_move_or_copy(begin(), end(), dst, guard);

    destroy_elements(data(), data() + size());
    deallocate_buffer();
  }

  // TODO rename range_move_or_copy to opt_move_or_copy

  void range_move_or_copy(pointer begin, pointer end, pointer dst)
  {
    typedef typename std::conditional<
         std::is_nothrow_move_constructible<T>::value
      || std::is_nothrow_copy_constructible<T>::value,
      null_construction_guard,
      construction_guard
    >::type guard_t;

    guard_t guard(dst, get_allocator_ref(), 0);

    range_move_or_copy(begin, end, dst, guard);

    guard.release();
  }

  template <typename Guard>
  void range_move_or_copy(pointer begin, pointer end, pointer dst, Guard& guard)
  {
    // if trivial copy and default allocator, memcpy
    if (allocator_traits::is_trivially_copyable)
    {
      std::memcpy(dst, begin, (end - begin) * sizeof(T));
    }
    else // guard needed
    {
      guarded_move_or_copy(dst, begin, end, guard);
    }
  }

  template <typename... Args>
  void resize_front_slow_path(size_type sz, size_type n, Args&&... args)
  {
    const size_type new_capacity = calculate_new_capacity(sz + back_free_capacity());
    pointer new_buffer = allocate(new_capacity);
    allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

    const size_type new_old_elem_index = new_capacity - size();
    const size_type new_elem_index = new_old_elem_index - n;

    construction_guard guard(new_buffer + new_elem_index, get_allocator_ref(), 0u);
    guarded_construct_n(new_buffer + new_elem_index, n, guard, std::forward<Args>(args)...);

    buffer_move_or_copy(new_buffer + new_old_elem_index, guard);

    guard.release();
    new_buffer_guard.release();

    _buffer = new_buffer;
    _storage._capacity = new_capacity;

    _back_index = new_old_elem_index + _back_index - _front_index;
    _front_index = new_elem_index;
  }

  template <typename... Args>
  void resize_back_slow_path(size_type sz, size_type n, Args&&... args)
  {
    const size_type new_capacity = calculate_new_capacity(sz + front_free_capacity());
    pointer new_buffer = allocate(new_capacity);
    allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

    construction_guard guard(new_buffer + _back_index, get_allocator_ref(), 0u);
    guarded_construct_n(new_buffer + _back_index, n, guard, std::forward<Args>(args)...);

    buffer_move_or_copy(new_buffer + _front_index);

    guard.release();
    new_buffer_guard.release();

    _buffer = new_buffer;
    _storage._capacity = new_capacity;

    _back_index = _back_index + n;
  }

  template <typename... Args>
  iterator emplace_slow_path(size_type new_elem_index, Args&&... args)
  {
    pointer position = begin() + new_elem_index;

    // prefer moving front to access memory forward if there are less elems to move
    bool prefer_move_front = 2*new_elem_index <= size();

    if (front_free_capacity() && (!back_free_capacity() || prefer_move_front))
    {
      BOOST_ASSERT(size() >= 1);

      // move things closer to the front a bit

      // avoid invalidating any reference in args later
      T tmp(std::forward<Args>(args)...);

      // construct at front - 1 from front (no guard)
      alloc_construct(begin() - 1, std::move(*begin()));

      // move front half left
      std::move(begin() + 1, position, begin());
      --_front_index;

      // move assign new elem before pos
      --position;
      *position = std::move(tmp);

      return position;
    }
    else if (back_free_capacity())
    {
      BOOST_ASSERT(size() >= 1);

      // move things closer to the end a bit

      // avoid invalidating any reference in args later
      T tmp(std::forward<Args>(args)...);

      // construct at back + 1 from back (no guard)
      alloc_construct(end(), std::move(back()));

      // move back half right
      std::move_backward(position, end() - 1, end());
      ++_back_index;

      // move assign new elem to pos
      *position = std::move(tmp);

      return position;
    }
    else
    {
      return emplace_reallocating_slow_path(prefer_move_front, new_elem_index, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  pointer emplace_reallocating_slow_path(bool make_front_free, size_type new_elem_index, Args&&... args)
  {
    // reallocate
    size_type new_capacity = calculate_new_capacity(capacity() + 1);
    pointer new_buffer = allocate(new_capacity);

    // guard allocation
    allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

    size_type new_front_index = (make_front_free)
      ? new_capacity - back_free_capacity() - size() - 1
      : _front_index;

    iterator new_begin = new_buffer + new_front_index;
    iterator new_position = new_begin + new_elem_index;
    iterator old_position = begin() + new_elem_index;

    // construct new element (and guard it)
    alloc_construct(new_position, std::forward<Args>(args)...);

    construction_guard second_half_guard(new_position, get_allocator_ref(), 1u);

    // move front-pos (possibly guarded)
    construction_guard first_half_guard(new_begin, get_allocator_ref(), 0);
    range_move_or_copy(begin(), old_position, new_begin, first_half_guard);

    // move pos+1-end (possibly guarded)
    range_move_or_copy(old_position, end(), new_position + 1, second_half_guard);

    // cleanup
    destroy_elements(begin(), end());
    deallocate_buffer();

    // release alloc and other guards
    second_half_guard.release();
    first_half_guard.release();
    new_buffer_guard.release();

    // rebind members
    _storage._capacity = new_capacity;
    _buffer = new_buffer;
    _back_index = new_front_index + size() + 1;
    _front_index = new_front_index;

    return new_position;
  }

  void reallocate_at(size_type new_capacity, size_type buffer_offset)
  {
    BOOST_ASSERT(new_capacity > storage_t::small_buffer_size);

    pointer new_buffer = allocate(new_capacity);
    allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

    buffer_move_or_copy(new_buffer + buffer_offset);

    new_buffer_guard.release();

    _buffer = new_buffer;
    _storage._capacity = new_capacity;

    _back_index = _back_index - _front_index + buffer_offset;
    _front_index = buffer_offset;

    BOOST_ASSERT(invariants_ok());
  }

  template <typename BidirIterator>
  iterator insert_range(const_iterator position, BidirIterator first, BidirIterator last)
  {
    size_type n = std::distance(first, last);

    if (position == end() && back_free_capacity() >= n) // fast path
    {
      for (; first != last; ++first)
      {
        unsafe_push_back(*first);
      }
      return end() - n;
    }
    else if (position == begin() && front_free_capacity() >= n) // secondary fast path
    {
      for (; first != last; ++first)
      {
        unsafe_push_front(*first);
      }
      return begin();
    }
    else
    {
      size_type new_elem_index = position - begin();
      return insert_range_slow_path(new_elem_index, first, last);
    }
  }

  template <typename BidirIterator>
  iterator insert_range_slow_path(size_type index, BidirIterator first, BidirIterator last)
  {
    size_type n = std::distance(first, last);

    // prefer moving front to access memory forward if there are less elems to move
    const bool prefer_move_front = 2 * index <= size();

    if (front_free_capacity() + back_free_capacity() >= n)
    {
      // if we move enough, it can be done without reallocation

      iterator middle = begin() + index;

      if (! prefer_move_front)
      {
        n -= insert_range_slow_path_near_back(middle, last, n);
      }

      if (n)
      {
        n -= insert_range_slow_path_near_front(middle, first, n);
      }

      if (n && prefer_move_front)
      {
        insert_range_slow_path_near_back(middle, last, n);
      }

      BOOST_ASSERT(first == last);

      return begin() + index;
    }
    else
    {
      return insert_range_reallocating_slow_path(prefer_move_front, index, first, n);
    }
  }

  template <typename BidirIterator>
  size_type insert_range_slow_path_near_front(iterator position, BidirIterator& first, size_type n)
  {
    size_type n_front = (std::min)(front_free_capacity(), n);
    iterator new_begin = begin() - n_front;
    iterator ctr_pos = new_begin;
    construction_guard ctr_guard(ctr_pos, get_allocator_ref(), 0u);

    while (ctr_pos != begin())
    {
      alloc_construct(ctr_pos++, *(first++));
      ctr_guard.increment_size(1u);
    }

    std::rotate(new_begin, ctr_pos, position);
    _front_index -= n_front;

    ctr_guard.release();

    return n_front;
  }

  template <typename BidirIterator>
  size_type insert_range_slow_path_near_back(iterator position, BidirIterator& last, size_type n)
  {
    size_type n_back = (std::min)(back_free_capacity(), n);
    iterator new_end = end() + n_back;
    iterator ctr_pos = new_end;
    construction_guard ctr_guard(ctr_pos, get_allocator_ref(), 0u);

    while (ctr_pos != end())
    {
      alloc_construct(--ctr_pos, *(--last));
      ctr_guard.increment_size_backwards(1u);
    }

    std::rotate(position, ctr_pos, new_end);
    _back_index += n_back;

    ctr_guard.release();

    return n_back;
  }

  template <typename Iterator>
  iterator insert_range_reallocating_slow_path(
    bool make_front_free, size_type new_elem_index, Iterator elems, size_type n
  )
  {
    // reallocate
    const size_type new_capacity = calculate_new_capacity(capacity() + n);
    pointer new_buffer = allocate(new_capacity);

    // guard allocation
    allocation_guard new_buffer_guard(new_buffer, get_allocator_ref(), new_capacity);

    const size_type new_front_index = (make_front_free)
      ? new_capacity - back_free_capacity() - size() - n
      : _front_index;

    const iterator new_begin = new_buffer + new_front_index;
    const iterator new_position = new_begin + new_elem_index;
    const iterator old_position = begin() + new_elem_index;

    // construct new element (and guard it)
    iterator second_half_position = new_position;
    construction_guard second_half_guard(second_half_position, get_allocator_ref(), 0u);

    for (size_type i = 0; i < n; ++i)
    {
      alloc_construct(second_half_position++, *(elems++));
      second_half_guard.increment_size(1u);
    }

    // move front-pos (possibly guarded)
    construction_guard first_half_guard(new_begin, get_allocator_ref(), 0);
    range_move_or_copy(begin(), old_position, new_begin, first_half_guard);

    // move pos+1-end (possibly guarded)
    range_move_or_copy(old_position, end(), second_half_position, second_half_guard);

    // cleanup
    destroy_elements(begin(), end());
    deallocate_buffer();

    // release alloc and other guards
    second_half_guard.release();
    first_half_guard.release();
    new_buffer_guard.release();

    // rebind members
    _storage._capacity = new_capacity;
    _buffer = new_buffer;
    _back_index = new_front_index + size() + n;
    _front_index = new_front_index;

    return new_position;
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
      alloc_construct(dest, *begin);
      copy_guard.increment_size(1u);
    }

    copy_guard.release();
  }

  template <typename... Args>
  void construct_n(pointer buffer, size_type n, Args&&... args)
  {
    construction_guard ctr_guard(buffer, get_allocator_ref(), 0u);

    guarded_construct_n(buffer, n, ctr_guard, std::forward<Args>(args)...);

    ctr_guard.release();
  }

  template <typename... Args>
  void guarded_construct_n(pointer buffer, size_type n, construction_guard& ctr_guard, Args&&... args)
  {
    for (size_type i = 0; i < n; ++i)
    {
      alloc_construct(buffer + i, std::forward<Args>(args)...);
      ctr_guard.increment_size(1u);
    }
  }

  static move_guard copy_front(devector& has_front, devector& needs_front) noexcept(
    std::is_nothrow_copy_constructible<T>::value || std::is_nothrow_move_constructible<T>::value
  )
  {
    pointer src = has_front.begin();
    pointer dst = needs_front._buffer + has_front._front_index;

    move_guard guard(
      src, has_front.get_allocator_ref(),
      dst, needs_front.get_allocator_ref()
    );

    needs_front.range_move_or_copy(
      src,
      has_front._buffer + (std::min)(has_front._back_index, needs_front._front_index),
      dst,
      guard
    );

    return guard;
  }

  static move_guard copy_back(devector& has_back, devector& needs_back) noexcept(
    std::is_nothrow_copy_constructible<T>::value || std::is_nothrow_move_constructible<T>::value
  )
  {
    size_type first_pos = (std::max)(has_back._front_index, needs_back._back_index);
    pointer src = has_back._buffer + first_pos;
    pointer dst = needs_back._buffer + first_pos;

    move_guard guard(
      src, has_back.get_allocator_ref(),
      dst, needs_back.get_allocator_ref()
    );

    needs_back.range_move_or_copy(src, has_back.end(), dst, guard);

    return guard;
  }

  static void swap_small_small(devector& a, devector& b) noexcept(
    (std::is_nothrow_copy_constructible<T>::value || std::is_nothrow_move_constructible<T>::value)
    // && nothrow_swappable
  )
  {
    // copy construct elems without pair in the other buffer

    move_guard front_guard;

    if (a._front_index < b._front_index)
    {
      front_guard = copy_front(a, b);
    }
    else if (a._front_index > b._front_index)
    {
      front_guard = copy_front(b, a);
    }

    move_guard back_guard;

    if (a._back_index > b._back_index)
    {
      back_guard = copy_back(a, b);
    }
    else if (a._back_index < b._back_index)
    {
      back_guard = copy_back(b, a);
    }

    // swap elems with pair in the other buffer

    std::swap_ranges(
      a._buffer + (std::min)((std::max)(a._front_index, b._front_index), a._back_index),
      a._buffer + (std::min)(a._back_index, b._back_index),
      b._buffer + (std::max)(a._front_index, b._front_index)
    );

    // no more exceptions

    front_guard.release();
    back_guard.release();

    // swap indices
    using std::swap;

    swap(a._front_index, b._front_index);
    swap(a._back_index, b._back_index);

    if (std::allocator_traits<Allocator>::propagate_on_container_swap::value)
    {
      swap(a.get_allocator_ref(), b.get_allocator_ref());
    }
  }

  static void swap_small_big(devector& small, devector& big) noexcept(
    std::is_nothrow_move_constructible<T>::value ||
    std::is_nothrow_copy_constructible<T>::value
  )
  {
    BOOST_ASSERT(small.is_small() == true);
    BOOST_ASSERT(big.is_small() == false);

    // small -> big
    big.range_move_or_copy(
      small.begin(), small.end(),
      big._storage.small_buffer_address() + small._front_index
    );

    small.destroy_elements(small.begin(), small.end());

    // big -> small
    small._buffer = big._buffer;
    big._buffer = big._storage.small_buffer_address();

    // big <-> small
    using std::swap;

    swap(small._storage._capacity, big._storage._capacity);
    swap(small._front_index, big._front_index);
    swap(small._back_index, big._back_index);

    if (std::allocator_traits<Allocator>::propagate_on_container_swap::value)
    {
      swap(small.get_allocator_ref(), big.get_allocator_ref());
    }
  }

  static void swap_big_big(devector& a, devector& b) noexcept
  {
    BOOST_ASSERT(a.is_small() == false);
    BOOST_ASSERT(b.is_small() == false);

    using std::swap;

    swap(a._storage._capacity, b._storage._capacity);
    swap(a._buffer, b._buffer);
    swap(a._front_index, b._front_index);
    swap(a._back_index, b._back_index);

    if (std::allocator_traits<Allocator>::propagate_on_container_swap::value)
    {
      swap(a.get_allocator_ref(), b.get_allocator_ref());
    }
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
    return storage_t::small_buffer_size && _storage._capacity <= storage_t::small_buffer_size;
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
  size_type capacity_alloc_count = 0;

  void reset_alloc_stats()
  {
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
