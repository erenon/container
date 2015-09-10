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

template <std::size_t SegmentSize>
struct stable_deque_policy
{
  static const std::size_t segment_size = SegmentSize;
};

template <
  typename T,
  typename Allocator = std::allocator<T>,
  typename StableDequePolicy = stable_deque_policy<512>
>
class stable_deque : Allocator
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

  static constexpr size_type segment_size = StableDequePolicy::segment_size;
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
    friend class stable_deque;
    friend class boost::iterator_core_access;

    ireference dereference()
    {
      return (*_p_segment)[_index];
    }

    ireference dereference() const
    {
      return (*_p_segment)[_index];
    }

    template <bool IsRhsConst>
    bool equal(const deque_iterator<IsRhsConst>& rhs) const
    {
      return _p_segment == rhs._p_segment
        &&   _index == rhs._index;
    }

    void increment()
    {
      ++_index;
      if (_index == segment_size)
      {
        ++_p_segment;
        _index = 0;
      }
    }

    void decrement()
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

    void advance(difference_type n)
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

    difference_type distance_to(const deque_iterator& b) const
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
    typedef typename std::conditional<IsConst, const stable_deque*, stable_deque*>::type container_pointer;

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
      if (map_size == 1 || _p_segment + 1 == map_end) { return _p_container->_back_index - _index; }
      return segment_size - _index;
    }

  private:
    friend class boost::iterator_core_access;

    ireference dereference()
    {
      return (*_p_segment)[_index];
    }

    ireference dereference() const
    {
      return (*_p_segment)[_index];
    }

    bool equal(const deque_segment_iterator& rhs) const
    {
      return _p_segment == rhs._p_segment;
    }

    void increment()
    {
      ++_p_segment;
      _index = 0;
    }

    void decrement()
    {
      --_p_segment;
      if (_p_segment == _p_container->_map.begin())
      {
        _index = _p_container->_back_index;
      }
    }

    void advance(difference_type n)
    {
      _p_segment += n;
      _index = 0;
    }

    difference_type distance_to(const deque_segment_iterator& b) const
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
  stable_deque() noexcept : stable_deque(Allocator()) {}

  explicit stable_deque(const Allocator& allocator) noexcept
    :Allocator(allocator),
     _front_index(),
     _back_index()
  {}

  explicit stable_deque(size_type n, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _map((n + segment_size - 1) / segment_size, reserve_only_tag{}),
     _front_index(),
     _back_index(n % segment_size)
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
      _back_index = to_destroy % segment_size;
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

  stable_deque(size_type n, const T& value, const Allocator& allocator = Allocator())
    :stable_deque(cvalue_iterator(value, n), cvalue_iterator(), allocator)
  {}

  template <class InputIterator,

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  typename std::enable_if<
    container_detail::is_input_iterator<InputIterator>::value
  ,int>::type = 0

#endif // ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  >
  stable_deque(InputIterator first, InputIterator last, const Allocator& allocator = Allocator())
    :stable_deque(allocator)
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
  stable_deque(ForwardIterator first, ForwardIterator last, const Allocator& allocator = Allocator())
    :Allocator(allocator),
     _map((std::distance(first, last) + segment_size - 1) / segment_size, reserve_only_tag{}),
     _front_index(),
     _back_index(std::distance(first, last) % segment_size)
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

      _back_index = 0;
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

#endif // ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

  stable_deque(const stable_deque& x)
    :stable_deque(x,
        allocator_traits::select_on_container_copy_construction(x.get_allocator_ref()))
  {}

  stable_deque(const stable_deque& rhs, const Allocator& allocator)
    :Allocator(allocator),
     _map(rhs._map.size(), reserve_only_tag{}),
     _front_index(rhs._front_index),
     _back_index(rhs._back_index)
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

        size_type offset = (segment_begin == rhs.segment_begin()) ? _front_index : 0;

        opt_copy(src, src_end, new_segment + offset);

        ++segment_begin;
      }
    }
    BOOST_CATCH(...)
    {
      deallocate_segment(_map.back());
      _map.pop_back();

      _back_index = (segment_begin == rhs.segment_begin()) ? _front_index : 0;
      destructor_impl();
      BOOST_RETHROW;
    }
    BOOST_CATCH_END

    BOOST_ASSERT(invariants_ok());
  }

  stable_deque(stable_deque&& x)
    :stable_deque(std::move(x), Allocator())
  {}

  stable_deque(stable_deque&& x, const Allocator& allocator)
    :Allocator(allocator),
     _map(std::move(x._map)),
     _front_index(x._front_index),
     _back_index(x._back_index)
  {
    x._front_index = 0;
    x._back_index = 0;
  }

  stable_deque(std::initializer_list<T> il, const Allocator& allocator = Allocator())
    :stable_deque(il.begin(), il.end(), allocator)
  {}

  ~stable_deque()
  {
    destructor_impl();
  }

  stable_deque& operator=(const stable_deque& x)
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

  stable_deque& operator=(stable_deque&& x) noexcept(allocator_traits::is_always_equal)
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
      swap(_front_index, x._front_index);
      swap(_back_index, x._back_index);
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

  stable_deque& operator=(std::initializer_list<T> il)
  {
    assign(il.begin(), il.end());
    return *this;
  }

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

  void assign(size_type n, const T& t)
  {
    assign(cvalue_iterator(t, n), cvalue_iterator());
  }

  void assign(std::initializer_list<T> il)
  {
    assign(il.begin(), il.end());
  }

  allocator_type get_allocator() const noexcept
  {
    return static_cast<const Allocator&>(*this);
  }

  // iterators:
  iterator begin() noexcept
  {
    return iterator{_map.begin(), _front_index};
  }

  const_iterator begin() const noexcept
  {
    return const_iterator{_map.begin(), _front_index};
  }

  iterator end() noexcept
  {
    auto back_it = _map.end();
    if (_back_index) { --back_it; }
    return iterator{back_it, _back_index};
  }

  const_iterator end() const noexcept
  {
    auto back_it = _map.cend();
    if (_back_index) { --back_it; }
    return const_iterator{back_it, _back_index};
  }

  reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(end());
  }

  const_reverse_iterator rbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() noexcept
  {
    return reverse_iterator(begin());
  }

  const_reverse_iterator rend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  const_iterator cbegin() const noexcept
  {
    return begin();
  }

  const_iterator cend() const noexcept
  {
    return end();
  }

  const_reverse_iterator crbegin() const noexcept
  {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator crend() const noexcept
  {
    return const_reverse_iterator(begin());
  }

  // segment iterators:
  segment_iterator segment_begin()
  {
    return segment_iterator{this, _map.begin(), _front_index};
  }

  const_segment_iterator segment_begin() const
  {
    return const_segment_iterator{this, _map.begin(), _front_index};
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
  bool empty() const noexcept
  {
    return _map.empty() || (_map.size() == 1 && _front_index == _back_index);
  }

  size_type size() const noexcept
  {
    if (_map.empty())
    {
      return 0;
    }
    else
    {
      size_type scaled_back = (_back_index + segment_size - 1) % segment_size + 1;
      return scaled_back - _front_index
        +    (_map.size() - 1) * segment_size;
    }
  }

  size_type max_size() const noexcept
  {
    return allocator_traits::max_size(get_allocator_ref());
  }

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
  }

  void shrink_to_fit()
  {
    if (empty() && !_map.empty())
    {
      deallocate_segment(_map.back());
      _map.pop_back();
      _map.shrink_to_fit();

      _front_index = 0;
      _back_index = 0;
    }
  }

  // element access:
  reference operator[](size_type n)
  {
    const size_type first_seg_size = segment_size - _front_index;
    if (n < first_seg_size)
    {
      return *(_map.front() + _front_index + n);
    }
    else
    {
      n -= first_seg_size;
      const size_type segment_index = n / segment_size + 1;
      const size_type elem_index = n % segment_size;
      return *(_map[segment_index] + elem_index);
    }
  }

  const_reference operator[](size_type n) const
  {
    const size_type first_seg_size = segment_size - _front_index;
    if (n < first_seg_size)
    {
      return *(_map.front() + _front_index + n);
    }
    else
    {
      n -= first_seg_size;
      const size_type segment_index = n / segment_size + 1;
      const size_type elem_index = n % segment_size;
      return *(_map[segment_index] + elem_index);
    }
  }

  reference at(size_type n)
  {
    if (n >= size()) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  const_reference at(size_type n) const
  {
    if (n >= size()) { throw_out_of_range("devector::at out of range"); }
    return (*this)[n];
  }

  reference front() noexcept
  {
    return *(_map.front() + _front_index);
  }

  const_reference front() const noexcept
  {
    return *(_map.front() + _front_index);
  }

  reference back() noexcept
  {
    size_type index = (_back_index) ? _back_index - 1 : segment_size - 1;
    return *(_map.back() + index);
  }

  const_reference back() const noexcept
  {
    size_type index = (_back_index) ? _back_index - 1 : segment_size - 1;
    return *(_map.back() + index);
  }

  // modifiers:
  template <class... Args>
  void emplace_front(Args&&... args)
  {
    if (front_free_capacity())
    {
      alloc_construct(_map.front() + (_front_index - 1), std::forward<Args>(args)...);
      --_front_index;
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
      alloc_construct(_map.back() + _back_index, std::forward<Args>(args)...);
      _back_index = (_back_index + 1) % segment_size;
    }
    else
    {
      emplace_back_slow_path(std::forward<Args>(args)...);
    }

    BOOST_ASSERT(invariants_ok());
  }

  template <class... Args>
  iterator emplace(const_iterator position, Args&&... args);

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

  iterator insert(const_iterator position, const T& x);
  iterator insert(const_iterator position, T&& x);

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
  iterator stable_insert(const_iterator position_hint, InputIterator first, InputIterator last);

  void pop_front()
  {
    BOOST_ASSERT(!empty());

    allocator_traits::destroy(get_allocator_ref(), &front());
    ++_front_index;

    if (_front_index == segment_size)
    {
      // TODO keep the last segment
      deallocate_segment(_map.front());
      _map.pop_front();

      _front_index = 0;
    }

    BOOST_ASSERT(invariants_ok());
  }

  void pop_back()
  {
    BOOST_ASSERT(!empty());

    // TODO XXX FIXME
    // DESIGN ERROR: can tell these from eachother:
    // [....]
    // ^     ^
    // [....]
    // ^
    // ^
    // Must use full iterator instead of simple indices

    allocator_traits::destroy(get_allocator_ref(), &back());

    if (_back_index)
    {
      --_back_index;

      if (_back_index == 0)
      {
        deallocate_segment(_map.back());
        _map.pop_back();
      }
    }
    else
    {
      _back_index = segment_size - 1;
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

    size_type n = ncfirst - begin();

    iterator new_end = std::move(nclast, end(), ncfirst);

    destroy_elements(new_end, end());

    const_map_iterator new_map_end = new_end._p_segment;
    if (new_end._index && new_map_end != _map.end()) { ++new_map_end; }
    _map.erase(new_map_end, _map.end());

    _back_index = new_end._index;

    BOOST_ASSERT(invariants_ok());

    return begin() + n;
  }

  void swap(stable_deque& rhs) noexcept
  {
    BOOST_ASSERT(
       !allocator_traits::propagate_on_container_swap::value
    || get_allocator_ref() == rhs.get_allocator_ref());

    using std::swap;
    swap(_map, rhs._map);
    swap(_front_index, rhs._front_index);
    swap(_back_index, rhs._back_index);
  }

  void clear() noexcept
  {
    destructor_impl();

    _map.clear();
    _front_index = 0;
    _back_index = 0;
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
    const value_type* const* const p_first_segment = _map.cbegin(); // why?
    size_type segment_index = it._p_segment - p_first_segment;
    return iterator(_map.begin() + segment_index, it._index);
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
    return _front_index;
  }

  size_type back_free_capacity() const noexcept
  {
    return (segment_size - _back_index) % segment_size;
  }

  template <typename... Args>
  void emplace_front_slow_path(Args&&... args)
  {
    BOOST_ASSERT(_front_index == 0);

    _map.reserve_front(new_map_capacity());

    pointer new_segment = allocate_segment();
    allocation_guard new_segment_guard(new_segment, get_allocator_ref(), segment_size);

    size_type new_front_index = segment_size - 1;

    alloc_construct(new_segment + new_front_index, std::forward<Args>(args)...);

    _map.push_front(new_segment); // noexcept because of reserve_front above
    _front_index = new_front_index;

    new_segment_guard.release();
  }

  template <typename... Args>
  void emplace_back_slow_path(Args&&... args)
  {
    BOOST_ASSERT(_back_index == 0);

    _map.reserve_back(new_map_capacity());

    pointer new_segment = allocate_segment();
    allocation_guard new_segment_guard(new_segment, get_allocator_ref(), segment_size);

    alloc_construct(new_segment, std::forward<Args>(args)...);

    _map.push_back(new_segment); // noexcept because of reserve_back above
    _back_index = 1;

    new_segment_guard.release();
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
      size_type input_size = last - first;
      size_type copy_size = segment_size;
      map_iterator dst_cur = _map.begin();
      map_iterator dst_end = _map.end();

      while (input_size && dst_cur != dst_end)
      {
        copy_size = (std::min)(input_size, size_type{segment_size});
        std::memcpy(*dst_cur, first, copy_size * sizeof(value_type));

        input_size -= copy_size;
        first += copy_size;
        ++dst_cur;
      }

      _front_index = 0;
      _back_index = copy_size % segment_size;
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

    _front_index = 0;
    _back_index = dst._index;
  }

  void erase_at_end(iterator from)
  {
    destroy_elements(from, end());

    const_map_iterator from_it = from._p_segment;

    if (from._index && from_it != _map.end())
    {
      ++from_it;
    }

    _map.erase(from_it, _map.end());
    _back_index = from._index;
  }

  template <typename InputIterator,
    typename std::enable_if<
      container_detail::is_input_iterator<InputIterator>::value
    ,int>::type = 0
  >
  iterator insert_range(const_iterator pos, InputIterator first, InputIterator last)
  {
    devector<T, devector_small_buffer_policy<0>, devector_growth_policy, Allocator> buffer(first, last);
    return insert_range(pos, buffer.begin(), buffer.end());
  }

  template <typename ForwardIterator,
    typename std::enable_if<
       container_detail::is_forward_iterator<ForwardIterator>::value
    ,int>::type = 0
  >
  iterator insert_range(const_iterator pos, ForwardIterator first, ForwardIterator last)
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
    return (! _map.empty() || (_front_index == 0 && _back_index == 0))
      &&    _front_index < segment_size
      &&    _back_index  < segment_size;
  }

  map_t _map;
  size_type _front_index;
  size_type _back_index;
};

template <class T, class AX, class PX, class AY, class PY>
bool operator==(const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  if (x.size() != y.size()) { return false; }
  return std::equal(x.begin(), x.end(), y.begin());
}

template <class T, class AX, class PX, class AY, class PY>
bool operator< (const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  return std::lexicographical_compare( x.begin(), x.end(),
                                       y.begin(), y.end() );
}

template <class T, class AX, class PX, class AY, class PY>
bool operator!=(const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  return !(x == y);
}

template <class T, class AX, class PX, class AY, class PY>
bool operator> (const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  return (y < x);
}

template <class T, class AX, class PX, class AY, class PY>
bool operator>=(const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  return !(x < y);
}

template <class T, class AX, class PX, class AY, class PY>
bool operator<=(const stable_deque<T, AX, PX>& x, const stable_deque<T, AY, PY>& y)
{
  return !(y < x);
}

// specialized algorithms:
template <class T, class Allocator, class P>
void swap(stable_deque<T, Allocator, P>& x, stable_deque<T, Allocator, P>& y) noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

}} // namespace boost::container

#endif // BOOST_CONTAINER_CONTAINER_FLEX_DEQUE_HPP
