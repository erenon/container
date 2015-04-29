#ifndef BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
#define BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP

#include <type_traits>
#include <memory>
#include <cstddef> // ptrdiff_t
#include <cstring> // memcpy

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
    :_capacity_begin(),
     _elem_begin(),
     _capacity_size(),
     _elem_size()
  {}

  ~devector()
  {
    if (_capacity_begin) { delete[] _capacity_begin; }
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
    if (_capacity_begin + _capacity_size - _elem_begin >= new_capacity) { return; }

    ptrdiff_t start = _elem_begin - _capacity_begin;
    reallocate_at(new_capacity, start);
  }

  void reserve_back(size_type new_capacity)
  {
    // back capacity > new_capacity
    ptrdiff_t need = _elem_begin + _elem_size - _capacity_begin - new_capacity;
    if (need <= 0) { return; }

    reallocate_at(new_capacity, need);
  }

  void push_front(const T& t)
  {
    if (_elem_begin <= _capacity_begin)
    {
      size_type new_capacity = (_capacity_size) ? _capacity_size * 2 : 10;
      size_type start = (_capacity_size) ? _capacity_size : 5; // TODO cap vs. size?
      reallocate_at(new_capacity, start);
    }

    --_elem_begin;
    *_elem_begin = t;
    ++_elem_size;
  }

  void push_back(const T& t)
  {
    if (_elem_begin + _elem_size >= _capacity_begin + _capacity_size)
    {
      size_type new_capacity = (_capacity_size) ? _capacity_size * 2 : 10;
      size_type start = _elem_begin - _capacity_begin;
      reallocate_at(new_capacity, start);
    }

    *(_elem_begin + _elem_size) = t;
    ++_elem_size;
  }

  void pop_front()
  {
    ++_elem_begin;
    --_elem_size;
  }

  void pop_back()
  {
    --_elem_size;
  }

  void resize(size_type count); // { resize_back(count); }
  void resize_front(size_type count);
  void resize_back(size_type count);

  pointer begin() { return _elem_begin; }
  pointer end()   { return _elem_begin + _elem_size; }

  size_type size() const { return _elem_size; }
  bool empty() const { return !_elem_size; }

  reference operator[](size_type i) { return *(_elem_begin + i); }

private:
  void reallocate_at(size_type new_capacity, size_type capacity_offset)
  {
    _capacity_size = new_capacity;
    pointer new_capacity_begin = new T[_capacity_size];

    memcpy(new_capacity_begin + capacity_offset, _elem_begin, _elem_size * sizeof(T));

    if (_capacity_begin) { delete[] _capacity_begin; }
    _capacity_begin = new_capacity_begin;

    _elem_begin = _capacity_begin + capacity_offset;
  }

  pointer _capacity_begin;
  pointer _elem_begin;

  size_type _capacity_size;
  size_type _elem_size;
};

}} // namespace boost::container

#endif // BOOST_CONTAINER_CONTAINER_DEVECTOR_HPP
