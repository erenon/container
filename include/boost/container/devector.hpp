#ifndef BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP

#include <type_traits>
#include <memory>
#include <cstddef> // ptrdiff_t
#include <cstring> // memcpy

#include <boost/assert.hpp>

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
 *  - small buffer optimization
 */

struct DefaultSmallBufferPolicy {};

template <
  typename T,
  typename Allocator = std::allocator<T>,
  typename SmallBufferPolicy = DefaultSmallBufferPolicy
>
class devector
{
public:
  typedef int difference_type;
  typedef unsigned int size_type;
  typedef typename Allocator::pointer pointer;
  typedef T& reference;

  devector()
    :_buffer(),
     _capacity(),
     _front_index(),
     _back_index()
  {}

  ~devector()
  {
    if (_buffer) { delete[] _buffer; }
  }

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
    *(_buffer + _front_index) = t;

    BOOST_ASSERT(invariants_ok());
  }

  void push_back(const T& t)
  {
    if (_back_index >= _capacity)
    {
      size_type new_capacity = (_capacity) ? _capacity * 2 : 10;
      reallocate_at(new_capacity, _front_index);
    }

    *(_buffer + _back_index) = t;
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

  pointer begin() { return _buffer + _front_index; }
  pointer end()   { return _buffer + _back_index; }

  size_type size() const { return _back_index - _front_index; }
  bool empty() const { return _front_index == _back_index; }

  reference operator[](size_type i) { return *(begin() + i); }

private:
  void reallocate_at(size_type new_capacity, size_type capacity_offset)
  {
    _capacity = new_capacity;
    pointer new_capacity_begin = new T[_capacity];

    #ifdef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
    ++capacity_alloc_count;
    #endif // BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

    memcpy(
      new_capacity_begin + capacity_offset,
      _buffer + _front_index,
      (_back_index - _front_index) * sizeof(T)
    );

    if (_buffer) { delete[] _buffer; }
    _buffer = new_capacity_begin;

    _back_index = _back_index - _front_index + capacity_offset;
    _front_index = capacity_offset;

    BOOST_ASSERT(invariants_ok());
  }

  bool invariants_ok()
  {
    return
       _front_index <= _back_index
    && _back_index <= _capacity;
  }

  pointer   _buffer;
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
