//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Benedek Thaler 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_CONTAINER_FLEX_DEQUE_HPP
#define BOOST_CONTAINER_CONTAINER_FLEX_DEQUE_HPP

#include <memory>

#include <boost/container/devector.hpp>
#include <boost/container/detail/iterators.hpp>
#include <boost/container/detail/destroyers.hpp>

#include <boost/iterator/iterator_facade.hpp>

namespace boost {
namespace container {

/** * Controls the deques implementation defined behavior */
template <std::size_t SegmentSize>
struct batch_deque_policy
{
  /**
   * The size of the allocated chunks in elements
   *
   * **Remark**: Using a power of two increases performance
   * because of the cheaper division and modulo operations.
   */
  static const std::size_t segment_size = SegmentSize;
};

/**
 * A standard conforming deque, providing more control over implementation details
 * (segment size), enabling better performance in certain use-cases (see Examples).
 *
 * In contrast with a standard vector, batch_deque stores the elements in multiple
 * chunks, called segments, thus avoiding moving or copying each element upon reallocation.
 *
 * Models the [SequenceContainer], [ReversibleContainer], and [AllocatorAwareContainer] concepts.
 *
 * **Requires**:
 *  - `T` shall be [MoveInsertable] into the batch_deque.
 *  - `T` shall be [Erasable] from any `batch_deque<T, P, Allocator>`.
 *  - `BatchDequePolicy` must model the concept with the same name.
 *
 * **Definition**: `T` is `NothrowConstructible` if it's either nothrow move constructible or
 * nothrow copy constructible.
 *
 * **Exceptions**: The exception specifications assume `T` is nothrow [Destructible].
 *
 * **Policies**:
 *
 * Models of the `BatchDequePolicy` concept must have the following static values:
 *
 * Type | Name | Description
 * -----|------|------------
 * `size_type` | `segment_size` | The number of elements a segment can hold
 *
 * @ref devector_small_buffer_policy models the `SmallBufferPolicy` concept.
 *
 * [SequenceContainer]: http://en.cppreference.com/w/cpp/concept/SequenceContainer
 * [ReversibleContainer]: http://en.cppreference.com/w/cpp/concept/ReversibleContainer
 * [AllocatorAwareContainer]: http://en.cppreference.com/w/cpp/concept/AllocatorAwareContainer
 * [Destructible]: http://en.cppreference.com/w/cpp/concept/Destructible
 */
template <
  typename T,
  typename BatchDequePolicy = batch_deque_policy<512>,
  typename Allocator = std::allocator<T>
>
class batch_deque : Allocator
{
  struct allocator_traits : public std::allocator_traits<Allocator>
  {
    // before C++14, std::allocator does not specify propagate_on_container_move_assignment,
    // std::allocator_traits sets it to false. Not something we would like to use.
    static constexpr bool propagate_on_move_assignment =
       std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
    || std::is_same<Allocator, std::allocator<T>>::value;

    // poor emulation of a C++17 feature
    static constexpr bool is_always_equal = std::is_same<Allocator, std::allocator<T>>::value;
  };

  static constexpr bool t_is_nothrow_constructible =
       std::is_nothrow_copy_constructible<T>::value
    || std::is_nothrow_move_constructible<T>::value;

  // TODO use is_trivially_copyable instead of is_pod when available
  static constexpr bool t_is_trivially_copyable = std::is_pod<T>::type::value;

public:

  typedef T value_type;
  typedef Allocator allocator_type;

  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef typename allocator_traits::pointer pointer;
  typedef typename allocator_traits::const_pointer const_pointer;

  typedef BOOST_CONTAINER_IMPDEF(std::size_t)    size_type;
  typedef BOOST_CONTAINER_IMPDEF(std::ptrdiff_t) difference_type;

private:

  typedef typename allocator_traits::template rebind_alloc<pointer> map_allocator;
  typedef devector<pointer, devector_small_buffer_policy<0>, devector_growth_policy, map_allocator> map_t;
  typedef typename map_t::iterator map_iterator;
  typedef typename map_t::const_iterator const_map_iterator;

  typedef container_detail::scoped_array_deallocator<Allocator> allocation_guard;

  // Destroy already constructed elements
  typedef typename container_detail::vector_value_traits<Allocator>::ArrayDestructor construction_guard;

  // When no guard needed
  typedef container_detail::null_scoped_destructor_n<Allocator> null_construction_guard;

  typedef constant_iterator<value_type, difference_type> cvalue_iterator;

  static constexpr size_type segment_size = BatchDequePolicy::segment_size;
  static_assert(segment_size > 1, "Segment size must be greater than 1");

  template <bool IsConst>
  class deque_iterator : public iterator_facade<
      deque_iterator<IsConst>,
      typename std::conditional<IsConst, const value_type, value_type>::type,
      random_access_traversal_tag
    >
  {
    typedef typename std::conditional<IsConst, const value_type, value_type>::type ivalue_type;
    typedef typename std::add_lvalue_reference<ivalue_type>::type ireference;
    typedef typename std::add_const<typename std::add_pointer<ivalue_type>::type>::type ipointer;
    typedef typename std::add_pointer<ipointer>::type segment_pointer;

  public:
    deque_iterator() noexcept {}

    deque_iterator(segment_pointer segment, size_type index) noexcept
      :_p_segment(segment),
       _index(index)
    {}

    deque_iterator(const deque_iterator<false>& x) noexcept
      :_p_segment(x._p_segment),
       _index(x._index)
    {}

    #ifdef BOOST_CONTAINER_TEST

    friend std::ostream& operator<<(std::ostream& out, const deque_iterator& i)
    {
      out << "[ " << i._p_segment << ", " << i._index << " ]";
      return out;
    }

    friend std::ostream& operator<<(std::ostream& out, const std::reverse_iterator<deque_iterator>& ri)
    {
      auto&& i = ri.base();
      out << "[ " << i._p_segment << ", " << i._index << " ]";
      return out;
    }

    #endif

  private:
    friend class batch_deque;
    friend class boost::iterator_core_access;

    ireference dereference() const noexcept
    {
      return (*_p_segment)[_index];
    }

    template <bool IsRhsConst>
    bool equal(const deque_iterator<IsRhsConst>& rhs) const noexcept
    {
      return _p_segment == rhs._p_segment
        &&   _index == rhs._index;
    }

    void increment() noexcept
    {
      ++_index;
      if (_index == segment_size)
      {
        ++_p_segment;
        _index = 0;
      }
    }

    void decrement() noexcept
    {
      if (_index != 0)
      {
        --_index;
      }
      else
      {
        --_p_segment;
        _index = segment_size - 1;
      }
    }

    void advance(difference_type n) noexcept
    {
      difference_type segment_diff;

      if (n >= 0)
      {
        segment_diff = (_index + n) / segment_size;
        _index = (_index + n) % segment_size;
      }
      else
      {
        constexpr difference_type sss = segment_size;
        const difference_type signed_index = _index;
        segment_diff = (1 - sss + signed_index + n) / sss;
        _index = (_index + (n % segment_size)) % segment_size;
      }

      _p_segment += segment_diff;
    }

    difference_type distance_to(const deque_iterator& b) const noexcept
    {
      return difference_type{segment_size}
        * (b._p_segment - _p_segment - 1) + b._index + (segment_size - _index);
    }

    segment_pointer _p_segment;
    size_type _index;
  };

  template <bool IsConst>
  class deque_segment_iterator : public iterator_facade<
      deque_segment_iterator<IsConst>,
      typename std::conditional<IsConst, const value_type, value_type>::type,
      random_access_traversal_tag
    >
  {
    typedef typename std::conditional<IsConst, const batch_deque*, batch_deque*>::type container_pointer;

    typedef typename std::conditional<IsConst, const value_type, value_type>::type ivalue_type;
    typedef typename std::add_lvalue_reference<ivalue_type>::type ireference;
    typedef typename std::add_const<typename std::add_pointer<ivalue_type>::type>::type ipointer;
    typedef typename std::add_pointer<ipointer>::type segment_pointer;

  public:
    deque_segment_iterator() noexcept {}

    deque_segment_iterator(container_pointer container, segment_pointer segment, size_type index) noexcept
      :_p_container(container),
       _p_segment(segment),
       _index(index)
    {}

    ipointer data() noexcept
    {
      return *_p_segment + _index;
    }

    const_pointer data() const noexcept
    {
      return *_p_segment + _index;
    }

    size_type data_size() const noexcept
    {
      const size_type map_size = _p_container->_map.size();
      const auto map_end = _p_container->_map.end();

      if (map_size == 0 || _p_segment == map_end) { return 0; }
      if (map_size == 1 || _p_segment + 1 == map_end) { return _p_container->_end._index - _index; }
      return segment_size - _index;
    }

  private:
    friend class boost::iterator_core_access;

    ireference dereference() const noexcept
    {
      return (*_p_segment)[_index];
    }

    bool equal(const deque_segment_iterator& rhs) const noexcept
    {
      return _p_segment == rhs._p_segment;
    }

    void increment() noexcept
    {
      ++_p_segment;
      _index = 0;
    }

    void decrement() noexcept
    {
      --_p_segment;
      if (_p_segment == _p_container->_map.begin())
      {
        _index = _p_container->_end_index;
      }
    }

    void advance(difference_type n) noexcept
    {
      _p_segment += n;
      _index = 0;
    }

    difference_type distance_to(const deque_segment_iterator& b) const noexcept
    {
      return b._p_segment - _p_segment;
    }

    container_pointer _p_container;
    segment_pointer _p_segment;
    size_type _index;
  };

public:

  typedef BOOST_CONTAINER_IMPDEF(deque_iterator<false>) iterator;
  typedef BOOST_CONTAINER_IMPDEF(deque_iterator<true>) const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  typedef deque_segment_iterator<false> segment_iterator;
  typedef deque_segment_iterator<true> const_segment_iterator;

  // construct/copy/destroy:

  /**
   * **Effects**: Constructs an empty deque.
   *
   * **Postcondition**: `empty()
   *
   * **Complexity**: Constant.
   */
  batch_deque() noexcept : batch_deque(Allocator())
  {
    BOOST_ASSERT(invariants_ok());
  }

  /**
   * **Effects**: Constructs an empty deque, using the specified allocator.
   *
   * **Postcondition**: `empty()
   *
   * **Complexity**: Constant.
   */
  explicit batch_deque(const Allocator& allocator) noexcept
    :Allocator(allocator),
     _begin(_map.begin(), 0u),
     _end(_begin)
  {
    BOOST_ASSERT(invariants_ok());
  }

  /**
   * [DefaultInsertable]: http://en.cppreference.com/w/cpp/concept/DefaultInsertable
   *
   * **Effects**: Constructs a deque with `n` default-inserted elements using the specified allocator.
   *
   * **Requires**: `T` shall be [DefaultInsertable] into `*this`.
   *
   * **Postcondition**: `size() == n.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in `n`.
   */
  explicit batch_deque(size_type n, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _map(ceil_int_div(n, segment_size), reserve_only_tag{}),
     _begin(_map.begin(), 0u),
     _end(_begin + n)
  {
    size_type i = n;

    BOOST_TRY
    {
      while (i)
      {
        pointer new_segment = allocate_segment();
        _map.unsafe_push_back(new_segment);

        size_type elems_in_segment = (std::min)(i, size_type{segment_size});

        for (size_type j = 0; j < elems_in_segment; ++j, --i)
        {
          alloc_construct(new_segment + j);
        }
      }
    }
    BOOST_CATCH(...)
    {
      const size_type to_destroy = n - i;
      _end = _begin + to_destroy;
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

  /**
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   *
   * **Effects**: Constructs a deque with `n` copies of `value`, using the specified allocator.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this`.
   *
   * **Postcondition**: `size() == n.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in `n`.
   */
  batch_deque(size_type n, const T& value, const Allocator& allocator = Allocator())
    :batch_deque(cvalue_iterator(value, n), cvalue_iterator(), allocator)
  {}

  /**
   * **Effects**: Constructs a deque equal to the range `[first,last)`, using the specified allocator.
   *
   * **Requires**: `T` shall be [EmplaceConstructible] into `*this` from `*first`. If the specified
   * iterator does not meet the forward iterator requirements, `T` shall also be [MoveInsertable]
   * into `*this`.
   *
   * **Postcondition**: `size() == std::distance(first, last)
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in the distance between `first` and `last`.
   *
   * **Remarks**: Each iterator in the range `[first,last)` shall be dereferenced exactly once,
   * unless an exception is thrown.
   * Although only `InputIterator` is required, different optimizations are provided if `first` and `last` meet the requirement of `ForwardIterator` or are pointers.
   *
   * [EmplaceConstructible]: http://en.cppreference.com/w/cpp/concept/EmplaceConstructible
   * [MoveInsertable]: http://en.cppreference.com/w/cpp/concept/MoveInsertable
   */
  template <class InputIterator,

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  typename std::enable_if<
    container_detail::is_input_iterator<InputIterator>::value
  ,int>::type = 0

#endif // ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  >
  batch_deque(InputIterator first, InputIterator last, const Allocator& allocator = Allocator())
    :batch_deque(allocator)
  {
    while (first != last)
    {
      push_back(*first++);
    }

    BOOST_ASSERT(invariants_ok());
  }

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  template <typename ForwardIterator, typename std::enable_if<
    container_detail::is_not_input_iterator<ForwardIterator>::value
  ,int>::type = 0>
  batch_deque(ForwardIterator first, ForwardIterator last, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _map(ceil_int_div(std::distance(first, last), segment_size), reserve_only_tag{}),
     _begin(_map.begin(), 0),
     _end(_begin + std::distance(first, last))
  {
    ForwardIterator segment_begin = first;

    BOOST_TRY
    {
      while (segment_begin != last)
      {
        const size_type cur_segment_size = (std::min)(std::distance(segment_begin,last), difference_type(segment_size));
        ForwardIterator segment_end = segment_begin;
        std::advance(segment_end, cur_segment_size);

        pointer new_segment = allocate_segment();
        _map.unsafe_push_back(new_segment);

        opt_copy(segment_begin, segment_end, new_segment);
        segment_begin = segment_end;
      }
    }
    BOOST_CATCH(...)
    {
      deallocate_segment(_map.back());
      _map.pop_back();

      _end = {_map.end(), 0};
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

#endif // ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  /**
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   *
   * **Effects**: Copy constructs a devector.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this`.
   *
   * **Postcondition**: `this->size() == x.size()`
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in the size of `x`.
   */
  batch_deque(const batch_deque& x)
    :batch_deque(x,
        allocator_traits::select_on_container_copy_construction(x.get_allocator_ref()))
  {}

  /**
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   *
   * **Effects**: Copy constructs a devector, using the specified allocator.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this`.
   *
   * **Postcondition**: `this->size() == x.size()
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in the size of `x`.
   */
  batch_deque(const batch_deque& rhs, const Allocator& allocator)
    :Allocator(allocator),
     _map(rhs._map.size(), reserve_only_tag{}),
     _begin(_map.begin(), rhs._begin._index),
     _end(_begin + rhs.size())
  {
    const_segment_iterator segment_begin = rhs.segment_begin();

    BOOST_TRY
    {
      while (segment_begin != rhs.segment_end())
      {
        const_pointer src = segment_begin.data();
        const_pointer src_end = src + segment_begin.data_size();

        pointer new_segment = allocate_segment();
        _map.unsafe_push_back(new_segment);

        size_type offset = (segment_begin == rhs.segment_begin()) ? _begin._index : 0;

        opt_copy(src, src_end, new_segment + offset);

        ++segment_begin;
      }
    }
    BOOST_CATCH(...)
    {
      deallocate_segment(_map.back());
      _map.pop_back();

      _end = (segment_begin == rhs.segment_begin()) ? _begin : iterator{_map.end(), 0};
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

  /**
   * **Effects**: Moves `rhs`'s resources to `*this`.
   *
   * **Postcondition**: `rhs` is left in an unspecified but valid state.
   *
   * **Complexity**: Constant.
   */
  batch_deque(batch_deque&& x) noexcept
    :Allocator(std::move(x.get_allocator_ref())),
     _map(std::move(x._map)),
     _begin(x._begin),
     _end(x._end)
  {
    x._end = x._begin = {x._map.begin(), 0};

    BOOST_ASSERT(invariants_ok());
    BOOST_ASSERT(x.invariants_ok());
  }

  /**
   * **Effects**: Moves `rhs`'s resources to `*this`,
   *
   * **Postcondition**: `rhs` is left in an unspecified but valid state.
   *
   * **Complexity**: Constant.
   */
  batch_deque(batch_deque&& x, const Allocator& allocator) noexcept
    :Allocator(allocator),
     _map(std::move(x._map)),
     _begin(x._begin),
     _end(x._end)
  {
    if (allocator == x.get_allocator_ref())
    {
      x._end = x._begin = {x._map.begin(), 0};
    }
    else
    {
      x._map = std::move(_map);
      _map.clear();
      _end = _begin = {_map.begin(), 0};

      auto first = std::make_move_iterator(x.begin());
      auto last = std::make_move_iterator(x.end());
      assign(first, last);
    }

    BOOST_ASSERT(invariants_ok());
    BOOST_ASSERT(x.invariants_ok());
  }

  /**
   * **Equivalent to**: `batch_deque(il.begin(), il.end())` or `batch_deque(il.begin(), il.end(), allocator)`.
   */
  batch_deque(std::initializer_list<T> il, const Allocator& allocator = Allocator())
    :batch_deque(il.begin(), il.end(), allocator)
  {}

  /**
   * **Effects**: Destroys the batch_deque. All stored values are destroyed and
   * used memory, if any, deallocated.
   *
   * **Complexity**: Linear in the size of `*this`.
   */
  ~batch_deque() noexcept
  {
    destructor_impl();
  }

  /**
   * **Effects**: Copies elements of `x` to `*this`. Previously
   * held elements get copy assigned to or destroyed.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this`.
   *
   * **Postcondition**: `this->size() == x.size()`, the elements of
   * `*this` are copies of elements in `x` in the same order.
   *
   * **Returns**: `*this`.
   *
   * **Exceptions**: Strong exception guarantee if `T` has noexcept copy constructor and
   * either has noexcept copy assignment operator.
   * or the allocator is not allowed to be propagated
   * ([propagate_on_container_copy_assignment] is false),
   * Basic exception guarantee otherwise.
   *
   * **Complexity**: Linear in the size of `x` and `*this`.
   *
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   * [propagate_on_container_copy_assignment]: http://en.cppreference.com/w/cpp/memory/allocator_traits
   */
  batch_deque& operator=(const batch_deque& x)
  {
    if (this == &x) { return *this; } // skip self

    if (allocator_traits::propagate_on_container_copy_assignment::value)
    {
      if (get_allocator_ref() != x.get_allocator_ref())
      {
        clear(); // new allocator cannot free existing storage
      }

      get_allocator_ref() = x.get_allocator_ref();
    }

    assign(x.begin(), x.end());

    return *this;
  }

  /**
   * [MoveInsertable]: http://en.cppreference.com/w/cpp/concept/MoveInsertable
   *
   * **Effects**: `*this` takes ownership of the resources of `x`.
   * The fate of the previously held elements are unspecified.
   *
   * **Requires**: `T` shall be [MoveInsertable] into `*this`.
   *
   * **Postcondition**: `x` is left in an unspecified but valid state.
   *
   * **Returns**: `*this`.
   *
   * **Exceptions**: Basic exception guarantee if not `noexcept`.
   *
   * Remark: If the allocator forbids propagation,
   * the contents of `x` is moved one-by-one instead of stealing the buffer.
   *
   * **Complexity**: Constant, if allocator can be propagated or the two
   * allocators compare equal, linear in the size of `x` and `*this` otherwise.
   */
  batch_deque& operator=(batch_deque&& x) noexcept(
      t_is_nothrow_constructible
   || allocator_traits::is_always_equal
   || allocator_traits::propagate_on_move_assignment
  )
  {
    constexpr bool copy_alloc = allocator_traits::propagate_on_move_assignment;
    const bool equal_alloc = (get_allocator_ref() == x.get_allocator_ref());

    if ((copy_alloc || equal_alloc))
    {
      if (copy_alloc)
      {
        get_allocator_ref() = std::move(x.get_allocator_ref());
      }

      using std::swap;

      swap(_map, x._map);
      swap(_begin, x._begin);
      swap(_end, x._end);
    }
    else
    {
      // if the allocator shouldn't be copied and they do not compare equal
      // we can't steal memory.

      if (copy_alloc)
      {
        get_allocator_ref() = std::move(x.get_allocator_ref());
      }

      auto first = std::make_move_iterator(x.begin());
      auto last = std::make_move_iterator(x.end());

      overwrite_buffer(first, last);

      while (first != last)
      {
        push_back(*first++);
      }
    }

    BOOST_ASSERT(invariants_ok());
    return *this;
  }

  /**
   * **Effects**: Copies elements of `il` to `*this`. Previously
   * held elements get copy assigned to or destroyed.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this` and [CopyAssignable].
   *
   * **Postcondition**: `this->size() == il.size()`, the elements of
   * `*this` are copies of elements in `il` in the same order.
   *
   * **Exceptions**: Strong exception guarantee if `T` is nothrow copy assignable
   * from `T` and `NothrowConstructible`, Basic exception guarantee otherwise.
   *
   * **Returns**: `*this`.
   *
   * **Complexity**: Linear in the size of `il` and `*this`.
   *
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   * [CopyAssignable]: http://en.cppreference.com/w/cpp/concept/CopyAssignable
   */
  batch_deque& operator=(std::initializer_list<T> il)
  {
    assign(il.begin(), il.end());
    return *this;
  }

  /**
   * **Effects**: Replaces elements of `*this` with a copy of `[first,last)`.
   * Previously held elements get copy assigned to or destroyed.
   *
   * **Requires**: `T` shall be [EmplaceConstructible] from `*first`.
   *
   * **Precondition**: `first` and `last` are not iterators into `*this`.
   *
   * **Postcondition**: `size() == N`, where `N` is the distance between `first` and `last`.
   *
   * **Exceptions**: Strong exception guarantee if `T` is nothrow copy assignable
   * from `*first` and `NothrowConstructible`, Basic exception guarantee otherwise.
   *
   * **Complexity**: Linear in the distance between `first` and `last`.
   *
   * **Remarks**: Each iterator in the range `[first,last)` shall be dereferenced exactly once,
   * unless an exception is thrown.
   *
   * [EmplaceConstructible]: http://en.cppreference.com/w/cpp/concept/EmplaceConstructible
   * [MoveInsertable]: http://en.cppreference.com/w/cpp/concept/MoveInsertable
   */
  template <typename Iterator>
  void assign(Iterator first, Iterator last)
  {
    overwrite_buffer(first, last);
    while (first != last)
    {
      push_back(*first++);
    }

    BOOST_ASSERT(invariants_ok());
  }

  /**
   * **Effects**: Replaces elements of `*this` with `n` copies of `t`.
   * Previously held elements get copy assigned to or destroyed.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this` and
   * [CopyAssignable].
   *
   * **Precondition**: `u` is not a reference into `*this`.
   *
   * **Postcondition**: `size() == n` and the elements of
   * `*this` are copies of `u`.
   *
   * **Exceptions**: Strong exception guarantee if `T` is nothrow copy assignable
   * from `u` and `NothrowConstructible`, Basic exception guarantee otherwise.
   *
   * **Complexity**: Linear in `n` and the size of `*this`.
   *
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   * [CopyAssignable]: http://en.cppreference.com/w/cpp/concept/CopyAssignable
   */
  void assign(size_type n, const T& t)
  {
    assign(cvalue_iterator(t, n), cvalue_iterator());
  }

  /** **Equivalent to**: `assign(il.begin(), il.end())`. */
  void assign(std::initializer_list<T> il)
  {
    assign(il.begin(), il.end());
  }

  /**
   * **Returns**: A copy of the allocator associated with the container.
   *
   * **Complexity**: Constant.
   */
  allocator_type get_allocator() const noexcept
  {
    return static_cast<const Allocator&>(*this);
  }

  // iterators:

  /**
   * **Returns**: A iterator pointing to the first element in the devector,
   * or the past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  iterator begin() noexcept
  {
    return _begin;
  }

  /**
   * **Returns**: A constant iterator pointing to the first element in the devector,
   * or the past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  const_iterator begin() const noexcept
  {
    return _begin;
  }

  /**
   * **Returns**: An iterator pointing past the last element of the container.
   *
   * **Complexity**: Constant.
   */
  iterator end() noexcept
  {
    return _end;
  }

  /**
   * **Returns**: A constant iterator pointing past the last element of the container.
   *
   * **Complexity**: Constant.
   */
  const_iterator end() const noexcept
  {
    return _end;
  }

  /**
   * **Returns**: A reverse iterator pointing to the first element in the reversed devector,
   * or the reverse past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(end());
  }

  /**
   * **Returns**: A constant reverse iterator
   * pointing to the first element in the reversed devector,
   * or the reverse past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  const_reverse_iterator rbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * **Returns**: A reverse iterator pointing past the last element in the
   * reversed container, or to the beginning of the reversed container if it's empty.
   *
   * **Complexity**: Constant.
   */
  reverse_iterator rend() noexcept
  {
    return reverse_iterator(begin());
  }

  /**
   * **Returns**: A constant reverse iterator pointing past the last element in the
   * reversed container, or to the beginning of the reversed container if it's empty.
   *
   * **Complexity**: Constant.
   */
  const_reverse_iterator rend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  /**
   * **Returns**: A constant iterator pointing to the first element in the devector,
   * or the past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  const_iterator cbegin() const noexcept
  {
    return begin();
  }

  /**
   * **Returns**: A constant iterator pointing past the last element of the container.
   *
   * **Complexity**: Constant.
   */
  const_iterator cend() const noexcept
  {
    return end();
  }

  /**
   * **Returns**: A constant reverse iterator
   * pointing to the first element in the reversed devector,
   * or the reverse past the end iterator if the devector is empty.
   *
   * **Complexity**: Constant.
   */
  const_reverse_iterator crbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  /**
   * **Returns**: A constant reverse iterator pointing past the last element in the
   * reversed container, or to the beginning of the reversed container if it's empty.
   *
   * **Complexity**: Constant.
   */
  const_reverse_iterator crend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  // segment iterators:
  segment_iterator segment_begin()
  {
    return segment_iterator{this, _map.begin(), _begin._index};
  }

  const_segment_iterator segment_begin() const
  {
    return const_segment_iterator{this, _map.begin(), _begin._index};
  }

  segment_iterator segment_end()
  {
    return segment_iterator{this, _map.end(), 0};
  }

  const_segment_iterator segment_end() const
  {
    return const_segment_iterator{this, _map.end(), 0};
  }

  // capacity:

  /**
   * **Returns**: True, if `size() == 0`, false otherwise.
   *
   * **Complexity**: Constant.
   */
  bool empty() const noexcept
  {
    return _begin == _end;
  }

  /**
   * **Returns**: The number of elements the container contains.
   *
   * **Complexity**: Constant.
   */
  size_type size() const noexcept
  {
    return _end - _begin;
  }

  /**
   * **Returns**: The maximum number of elements the container could possibly hold.
   *
   * **Complexity**: Constant.
   */
  size_type max_size() const noexcept
  {
    return allocator_traits::max_size(get_allocator_ref());
  }

  /**
   * [DefaultConstructible]: http://en.cppreference.com/w/cpp/concept/DefaultConstructible
   *
   * **Effects**: If `sz` is greater than the size of `*this`,
   * additional value-initialized elements are inserted
   * to the back. Invalidates iterators if segment allocation is needed.
   * If `sz` is smaller than than the size of `*this`,
   * elements are popped from the back.
   *
   * **Requires**: T shall be [DefaultConstructible].
   *
   * **Postcondition**: `sz == size()`.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in the size of `*this` and `sz`.
   */
  void resize(size_type sz)
  {
    if (sz >= size())
    {
      for (size_type diff = sz - size(); diff; --diff)
      {
        emplace_back();
      }
    }
    else
    {
      iterator new_end = begin() + sz;
      erase_at_end(new_end);
    }
  }

  /**
   * [CopyInsertable]: http://en.cppreference.com/w/cpp/concept/CopyInsertable
   *
   * **Effects**: If `sz` is greater than the size of `*this`,
   * copies of `c` are inserted to the back. Invalidates iterators if
   * segment allocation is needed.
   * If `sz` is smaller than than the size of `*this`,
   * elements are popped from the back.
   *
   * **Postcondition**: `sz == size()`.
   *
   * **Requires**: `T` shall be [CopyInsertable] into `*this`.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Linear in the size of `*this` and `sz`.
   */
  void resize(size_type sz, const T& c)
  {
    if (sz >= size())
    {
      for (size_type diff = sz - size(); diff; --diff)
      {
        push_back(c);
      }
    }
    else
    {
      iterator new_end = begin() + sz;
      erase_at_end(new_end);
    }

    BOOST_ASSERT(invariants_ok());
  }

  /**
   * **Effects**: Deallocates empty segments.
   *
   * **Complexity**: Constant.
   *
   * **Remarks**: Any empty segment is retained only if it's
   * the last segment of the container. Therefore, this method has
   * no effects if the container is not empty.
   */
  void shrink_to_fit() noexcept
  {
    if (empty() && ! _map.empty())
    {
      deallocate_segment(_map.back());
      _map.pop_back();
      _map.shrink_to_fit();

      _end = _begin = {_map.begin(), 0};
    }

    BOOST_ASSERT(invariants_ok());
  }

  // element access:

  /**
   * **Returns**: A reference to the `n`th element in the container.
   *
   * **Precondition**: `n < size()`.
   *
   * **Complexity**: Constant.
   */
  reference operator[](size_type n)
  {
    BOOST_ASSERT(n < size());

    return *(_begin + n);
  }

  /**
   * **Returns**: A constant reference to the `n`th element in the container.
   *
   * **Precondition**: `n < size()`.
   *
   * **Complexity**: Constant.
   */
  const_reference operator[](size_type n) const
  {
    BOOST_ASSERT(n < size());

    return *(_begin + n);
  }

  /**
   * **Returns**: A reference to the `n`th element in the container.
   *
   * **Throws**: `std::out_of_range`, if `n >= size()`.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Constant.
   */
  reference at(size_type n)
  {
    if (n >= size()) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  /**
   * **Returns**: A constant reference to the `n`th element in the devector.
   *
   * **Throws**: `std::out_of_range`, if `n >= size()`.
   *
   * **Exceptions**: Strong exception guarantee.
   *
   * **Complexity**: Constant.
   */
  const_reference at(size_type n) const
  {
    if (n >= size()) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  /**
   * **Returns**: A reference to the first element in the devector.
   *
   * **Precondition**: `!empty()`.
   *
   * **Complexity**: Constant.
   */
  reference front() noexcept
  {
    BOOST_ASSERT(!empty());

    return *_begin;
  }

  /**
   * **Returns**: A constant reference to the first element in the devector.
   *
   * **Precondition**: `!empty()`.
   *
   * **Complexity**: Constant.
   */
  const_reference front() const noexcept
  {
    BOOST_ASSERT(!empty());

    return *_begin;
  }

  /**
   * **Returns**: A reference to the last element in the devector.
   *
   * **Precondition**: `!empty()`.
   *
   * **Complexity**: Constant.
   */
  reference back() noexcept
  {
    BOOST_ASSERT(!empty());

    iterator back_it = _end - 1;
    return *back_it;
  }

  /**
   * **Returns**: A constant reference to the last element in the devector.
   *
   * **Precondition**: `!empty()`.
   *
   * **Complexity**: Constant.
   */
  const_reference back() const noexcept
  {
    BOOST_ASSERT(!empty());

    iterator back_it = _end - 1;
    return *back_it;
  }

  // modifiers:
  template <class... Args>
  void emplace_front(Args&&... args)
  {
    if (front_free_capacity())
    {
      alloc_construct(_map.front() + (_begin._index - 1), std::forward<Args>(args)...);
      --_begin;
    }
    else
    {
      emplace_front_slow_path(std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  void emplace_back(Args&&... args)
  {
    if (back_free_capacity())
    {
      alloc_construct(_map.back() + _end._index, std::forward<Args>(args)...);
      ++_end;
    }
    else
    {
      emplace_back_slow_path(std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  iterator emplace(const_iterator position, Args&&... args)
  {
    iterator result;

    if (position == cend())
    {
      emplace_back(std::forward<Args>(args)...);
      result = end() - 1;
    }
    else if (position == cbegin())
    {
      emplace_front(std::forward<Args>(args)...);
      result = begin();
    }
    else
    {
      result = emplace_slow_path(
        unconst_iterator(position),
        std::forward<Args>(args)...
      );
    }

    BOOST_ASSERT(invariants_ok());
    return result;
  }

  void push_front(const T& x)
  {
    emplace_front(x);
  }

  void push_front(T&& x)
  {
    emplace_front(std::move(x));
  }

  void push_back(const T& x)
  {
    emplace_back(x);
  }

  void push_back(T&& x)
  {
    emplace_back(std::move(x));
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

  template <class InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last)
  {
    return insert_range(position, first, last);
  }

  iterator insert(const_iterator position, std::initializer_list<T> il)
  {
    return insert_range(position, il.begin(), il.end());
  }

  template <class InputIterator>
  iterator stable_insert(const_iterator position_hint, InputIterator first, InputIterator last)
  {
    if (position_hint == _end || position_hint == _begin)
    {
      return insert(position_hint, first, last);
    }
    else
    {
      batch_deque tmp(first, last);
      tmp.resize(tmp.size() + tmp.back_free_capacity());

      const const_map_iterator hint_segment = unconst_iterator(position_hint)._p_segment;
      map_iterator result_segment = _map.insert(hint_segment, tmp._map.begin(), tmp._map.end());
      fix_iterators();

      tmp._map.clear();
      tmp._end = tmp._begin = {tmp._map.begin(), 0};

      BOOST_ASSERT(invariants_ok());
      return iterator{result_segment, 0};
    }
  }

  void pop_front()
  {
    BOOST_ASSERT(!empty());

    allocator_traits::destroy(get_allocator_ref(), &front());
    ++_begin;

    if (_begin._index == 0)
    {
      // TODO keep the last segment
      deallocate_segment(_map.front());
      _map.pop_front();
    }

    BOOST_ASSERT(invariants_ok());
  }

  void pop_back()
  {
    BOOST_ASSERT(!empty());

    allocator_traits::destroy(get_allocator_ref(), &back());
    --_end;

    if (_end._index == 0)
    {
      // TODO keep the last segment
      deallocate_segment(_map.back());
      _map.pop_back();
    }

    BOOST_ASSERT(invariants_ok());
  }

  iterator erase(const_iterator position)
  {
    return erase(position, position + 1);
  }

  iterator erase(const_iterator first, const_iterator last)
  {
    iterator ncfirst = unconst_iterator(first);
    iterator nclast  = unconst_iterator(last);

    // TODO possible optimization: decide on erased end
    // based on range position

    const size_type n = ncfirst - begin();

    iterator new_end = std::move(nclast, end(), ncfirst);
    erase_at_end(new_end);

    BOOST_ASSERT(invariants_ok());

    return begin() + n;
  }

  void swap(batch_deque& rhs) noexcept
  {
    BOOST_ASSERT(
       !allocator_traits::propagate_on_container_swap::value
    || get_allocator_ref() == rhs.get_allocator_ref());

    using std::swap;
    swap(_map, rhs._map);
    swap(_begin, rhs._begin);
    swap(_end, rhs._end);
  }

  void clear() noexcept
  {
    destructor_impl();

    _map.clear();
    _end = _begin = {_map.begin(), 0};
  }

private:

  allocator_type& get_allocator_ref()
  {
    return static_cast<allocator_type&>(*this);
  }

  const allocator_type& get_allocator_ref() const
  {
    return static_cast<const allocator_type&>(*this);
  }

  void destructor_impl()
  {
    destroy_elements(begin(), end());
    deallocate_segments();
  }

  void destroy_elements(iterator first, iterator last)
  {
    for (; first != last; ++first)
    {
      allocator_traits::destroy(get_allocator_ref(), &*first);
    }
  }

  void deallocate_segments()
  {
    for (pointer segment : _map)
    {
      deallocate_segment(segment);
    }
  }

  pointer allocate_segment()
  {
    return allocator_traits::allocate(get_allocator_ref(), segment_size);
  }

  void deallocate_segment(pointer segment)
  {
    allocator_traits::deallocate(get_allocator_ref(), segment, segment_size);
  }

  iterator unconst_iterator(const const_iterator& it) noexcept
  {
    size_type offset = it - cbegin();
    return begin() + offset;
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

  size_type front_free_capacity() const noexcept
  {
    return _begin._index;
  }

  size_type back_free_capacity() const noexcept
  {
    return (segment_size - _end._index) % segment_size;
  }

  template <typename... Args>
  void emplace_front_slow_path(Args&&... args)
  {
    BOOST_ASSERT(front_free_capacity() == 0);

    _map.reserve_front(new_map_capacity());
    fix_iterators();

    pointer new_segment = allocate_segment();
    allocation_guard new_segment_guard(new_segment, get_allocator_ref(), segment_size);

    size_type new_front_index = segment_size - 1;

    alloc_construct(new_segment + new_front_index, std::forward<Args>(args)...);

    _map.push_front(new_segment); // noexcept because of reserve_front above
    _begin = {_map.begin(), new_front_index};

    new_segment_guard.release();
  }

  template <typename... Args>
  void emplace_back_slow_path(Args&&... args)
  {
    BOOST_ASSERT(back_free_capacity() == 0);

    _map.reserve_back(new_map_capacity());
    fix_iterators();

    pointer new_segment = allocate_segment();
    allocation_guard new_segment_guard(new_segment, get_allocator_ref(), segment_size);

    alloc_construct(new_segment, std::forward<Args>(args)...);

    _map.push_back(new_segment); // noexcept because of reserve_back above
    _end = {&_map.back(), 1};

    new_segment_guard.release();
  }

  template <class... Args>
  iterator emplace_slow_path(iterator pos, Args&&... args)
  {
    const size_type pos_index = pos - begin();
    const bool prefer_ins_back = (pos_index >= size() / 2);

    if (prefer_ins_back)
    {
      emplace_back(std::forward<Args>(args)...);
      iterator npos = begin() + pos_index;
      std::rotate(npos, end() - 1, end());
      return npos;
    }
    else
    {
      emplace_front(std::forward<Args>(args)...);
      iterator npos = begin() + pos_index;
      std::rotate(begin(), begin() + 1, npos + 1);
      return npos;
    }
  }

  void fix_iterators()
  {
    _begin = {_map.begin(), _begin._index};
    _end = (_end._index) ? iterator{&_map.back(), _end._index} : iterator{_map.end(), 0};
  }

  size_type new_map_capacity() const
  {
    size_type map_size = _map.size();
    return (map_size) ? map_size + map_size / 2 + 1 : 4;
  }

  template <typename Iterator>
  void opt_copy(Iterator begin, Iterator end, pointer dst)
  {
    typedef typename std::conditional<
      std::is_nothrow_copy_constructible<T>::value,
      null_construction_guard,
      construction_guard
    >::type guard_t;

    guard_t guard(dst, get_allocator_ref(), 0);

    opt_copy(begin, end, dst, guard);

    guard.release();
  }

  template <typename Iterator, typename Guard>
  void opt_copy(Iterator begin, Iterator end, pointer dst, Guard& guard)
  {
    while (begin != end)
    {
      alloc_construct(dst++, *begin++);
      guard.increment_size(1u);
    }
  }

  template <typename Guard>
  void opt_copy(const_pointer begin, const_pointer end, pointer dst, Guard& guard)
  {
    // if trivial copy and default allocator, memcpy
    if (t_is_trivially_copyable)
    {
      std::memcpy(dst, begin, (end - begin) * sizeof(T));
    }
    else // guard needed
    {
      while (begin != end)
      {
        alloc_construct(dst++, *begin++);
        guard.increment_size(1u);
      }
    }
  }

  void overwrite_buffer(const_pointer& first, const_pointer last)
  {
    if (t_is_trivially_copyable)
    {
      const const_pointer first_origi = first;
      size_type input_size = last - first;
      size_type copy_size = segment_size;
      map_iterator dst_cur = _map.begin();
      const map_iterator dst_end = _map.end();

      while (input_size && dst_cur != dst_end)
      {
        copy_size = (std::min)(input_size, size_type{segment_size});
        std::memcpy(*dst_cur, first, copy_size * sizeof(value_type));

        input_size -= copy_size;
        first += copy_size;
        ++dst_cur;
      }

      BOOST_ASSERT(_begin._p_segment == _map.begin());
      _begin._index = 0;
      _end = _begin + std::distance(first_origi, first);

      _map.erase(dst_cur, dst_end);
    }
    else
    {
      overwrite_buffer_impl(first, last);
    }
  }

  template <typename Iterator>
  void overwrite_buffer(Iterator& first, const Iterator last)
  {
    overwrite_buffer_impl(first, last);
  }

  template <typename Iterator>
  void overwrite_buffer_impl(Iterator& first, const Iterator last)
  {
    if (empty()) { return; }

    // fill free front
    iterator dst(_map.begin(), 0);
    construction_guard front_guard(&*dst, get_allocator_ref(), 0u);

    while (first != last && dst != begin())
    {
      alloc_construct(&*dst, *first++);
      ++dst;
      front_guard.increment_size(1u);
    }

    front_guard.release();

    if (dst != begin())
    {
      erase_at_end(begin());
    }
    else
    {
      // assign to existing elements
      for (; first != last && dst != end(); ++dst, ++first)
      {
        *dst = *first;
      }

      erase_at_end(dst);
    }

    BOOST_ASSERT(_begin._p_segment == _map.begin());
    _begin._index = 0;
    _end = dst;
  }

  void erase_at_end(iterator new_end)
  {
    destroy_elements(new_end, end());

    const_map_iterator new_map_end = new_end._p_segment;

    if (new_end._index)
    {
      BOOST_ASSERT(new_map_end != _map.end());
      ++new_map_end;
    }

    _map.erase(new_map_end, _map.end());
    _end = new_end;
  }

  template <typename InputIterator,
    typename std::enable_if<
         container_detail::is_input_iterator<InputIterator>::value
     ||  container_detail::is_forward_iterator<InputIterator>::value
    ,int>::type = 0
  >
  iterator insert_range(const_iterator pos, InputIterator first, InputIterator last)
  {
    const size_type pos_index = pos - cbegin();
    const size_type old_end_index = size();

    while (first != last)
    {
      push_back(*first++);
    }

    iterator ncpos = begin() + pos_index;
    iterator old_end = begin() + old_end_index;
    std::rotate(ncpos, old_end, end());

    BOOST_ASSERT(invariants_ok());

    return ncpos;
  }

  template <typename BidirIterator,
    typename std::enable_if<
       ! container_detail::is_input_iterator<BidirIterator>::value
    && ! container_detail::is_forward_iterator<BidirIterator>::value
    ,int>::type = 0
  >
  iterator insert_range(const_iterator pos, BidirIterator first, BidirIterator last)
  {
    const size_type pos_index = pos - cbegin();
    const bool prefer_ins_back = (pos_index >= size() / 2);

    if (prefer_ins_back)
    {
      const size_type old_end_index = size();

      while (first != last)
      {
        push_back(*first++);
      }

      iterator ncpos = begin() + pos_index;
      iterator old_end = begin() + old_end_index;
      std::rotate(ncpos, old_end, end());
      return ncpos;
    }
    else
    {
      size_type n = 0;

      while (last != first)
      {
        push_front(*(--last));
        ++n;
      }

      iterator old_begin = begin() + n;
      iterator ncpos = old_begin + pos_index;
      std::rotate(begin(), old_begin, ncpos);
      return begin() + pos_index;
    }
  }

  bool invariants_ok()
  {
    return (! _map.empty() || (_begin == _end))
      &&    _begin <= _end
      &&    _begin._p_segment == _map.begin()
      &&    _begin._index < segment_size
      &&    (_end._index ? *_end._p_segment == _map.back() : _end._p_segment == _map.end())
      &&    _end._index  < segment_size;
  }

  template <typename N>
  static constexpr N ceil_int_div(N n, size_type d)
  {
    return (n + d - 1) / d;
  }

  map_t _map;
  iterator _begin;
  iterator _end;
};

template <class T, class PX, class AX, class PY, class AY>
bool operator==(const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  if (x.size() != y.size()) { return false; }
  return std::equal(x.begin(), x.end(), y.begin());
}

template <class T, class PX, class AX, class PY, class AY>
bool operator< (const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  return std::lexicographical_compare( x.begin(), x.end(),
                                       y.begin(), y.end() );
}

template <class T, class PX, class AX, class PY, class AY>
bool operator!=(const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  return !(x == y);
}

template <class T, class PX, class AX, class PY, class AY>
bool operator> (const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  return (y < x);
}

template <class T, class PX, class AX, class PY, class AY>
bool operator>=(const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  return !(x < y);
}

template <class T, class PX, class AX, class PY, class AY>
bool operator<=(const batch_deque<T, PX, AX>& x, const batch_deque<T, PY, AY>& y)
{
  return !(y < x);
}

// specialized algorithms:
template <class T, class P, class Allocator>
void swap(batch_deque<T, P, Allocator>& x, batch_deque<T, P, Allocator>& y) noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

}} // namespace boost::container

#endif // BOOST_CONTAINER_CONTAINER_FLEX_DEQUE_HPP
