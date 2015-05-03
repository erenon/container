#ifndef BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP

#include <type_traits>
#include <memory>
#include <cstddef> // ptrdiff_t
#include <cstring> // memcpy
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/aligned_storage.hpp>

namespace boost {
namespace container {

/*
 * Notes:
 * Ignored for now:
 *  - full interface
 *  - noexcept
 *  - proper overload resolution
 *  - allocator
 *  - iterator
 */

/**
 * N: number of objects in the small buffer
 *
 * TODO: separate front/back store
 */
template <unsigned N>
struct devector_store_n
{
  BOOST_STATIC_CONSTANT(unsigned, size = N);
};

template <
  typename T,
  typename Allocator = std::allocator<T>,
  typename SmallBufferPolicy = devector_store_n<0>
>
class devector
{
public:
  typedef int difference_type;
  typedef unsigned int size_type;
  typedef typename Allocator::pointer pointer;
  typedef T& reference;

  devector()
    :_capacity(storage_t::size),
     _front_index(),
     _back_index()
  {}

  // for scaffolding only
  static_assert(std::is_pod<T>::value, "Must be POD now");

  /*
   * reserve promise:
   * after reserve(n), n - size() push_x will not allocate
   */

  void reserve(size_type new_capacity) { reserve_back(new_capacity); }

  void reserve_front(size_type new_capacity)
  {
    // front capacity > new_capacity
    if (_capacity - _front_index >= new_capacity) { return; }

    reallocate_at(new_capacity, _front_index);

    BOOST_ASSERT(invariants_ok());
  }

  void reserve_back(size_type new_capacity)
  {
    // back capacity >= new_capacity
    if (_capacity - _front_index >= new_capacity) { return; }

    reallocate_at(new_capacity, _front_index);

    BOOST_ASSERT(invariants_ok());
  }

  void push_front(const T& t)
  {
    if (_front_index <= 0)
    {
      size_type new_capacity = (_capacity) ? _capacity * 2 : 10;
      size_type start = (_capacity) ? _capacity : 5; // TODO cap vs. size?
      reallocate_at(new_capacity, start);
    }

    --_front_index;
    *(_storage._buffer + _front_index) = t;

    BOOST_ASSERT(invariants_ok());
  }

  void push_back(const T& t)
  {
    if (_back_index >= _capacity)
    {
      size_type new_capacity = (_capacity) ? _capacity * 2 : 10;
      reallocate_at(new_capacity, _front_index);
    }

    *(_storage._buffer + _back_index) = t;
    ++_back_index;

    BOOST_ASSERT(invariants_ok());
  }

  void pop_front()
  {
    ++_front_index;
    BOOST_ASSERT(invariants_ok());
  }

  void pop_back()
  {
    --_back_index;
    BOOST_ASSERT(invariants_ok());
  }

  void resize(size_type count); // { resize_back(count); }
  void resize_front(size_type count);
  void resize_back(size_type count);

  pointer begin() { return _storage._buffer + _front_index; }
  pointer end()   { return _storage._buffer + _back_index; }

  size_type size() const { return _back_index - _front_index; }
  bool empty() const { return _front_index == _back_index; }

  reference operator[](size_type i) { return *(begin() + i); }

private:
  void reallocate_at(size_type new_capacity, size_type capacity_offset)
  {
    pointer new_capacity_begin;
    if (new_capacity <= storage_t::size)
    {
      _capacity = storage_t::size;
      new_capacity_begin = _storage.address();
    }
    else
    {
      _capacity = new_capacity;
      new_capacity_begin = new T[_capacity];

      #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
      ++capacity_alloc_count;
      #endif // BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
    }

    memcpy(
      new_capacity_begin + capacity_offset,
      _storage._buffer + _front_index,
      (_back_index - _front_index) * sizeof(T)
    );

    _storage.reset(new_capacity_begin);

    _back_index = _back_index - _front_index + capacity_offset;
    _front_index = capacity_offset;

    BOOST_ASSERT(invariants_ok());
  }

  bool invariants_ok()
  {
    return
       _front_index <= _back_index
    && _back_index <= _capacity
    && storage_t::size <= _capacity;
  }

  // Small buffer

  typedef boost::aligned_storage<
    sizeof(T) * SmallBufferPolicy::size,
    std::alignment_of<T>::value
  > small_buffer;

  // Achieve optimal space by leveraging EBO
  struct storage_t : small_buffer
  {
    BOOST_STATIC_CONSTANT(unsigned, size = SmallBufferPolicy::size);

    storage_t()
      :_buffer(address())
    {}

    ~storage_t()
    {
      if (_buffer != address()) { delete[] _buffer; }
    }

    T* address()
    {
      return static_cast<T*>(small_buffer::address());
    }

    const T* address() const
    {
      return static_cast<const T*>(small_buffer::address());
    }

    void reset(T* new_buffer)
    {
      if (_buffer != address()) { delete[] _buffer; }
      _buffer = new_buffer;
    }

    T* _buffer;
  };

  storage_t _storage;
  size_type _capacity;

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

}} // namespace boost::container

#endif // BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
