//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Benedek Thaler 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <cstring> // memcmp
#include <iostream>
#include <vector>
#include <forward_list>

#define BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
#include <boost/container/devector.hpp>
#undef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

#include <boost/algorithm/cxx14/equal.hpp>
#include <boost/move/core.hpp> // BOOST_MOVABLE_BUT_NOT_COPYABLE
#include <boost/utility/compare_pointees.hpp>

using namespace boost::container;

struct test_exception {};

struct test_elem_throw
{
  static int throw_on_ctor_after /*= -1*/;
  static int throw_on_copy_after /*= -1*/;
  static int throw_on_move_after /*= -1*/;

  static void on_ctor_after(int x) { throw_on_ctor_after = x; }
  static void on_copy_after(int x) { throw_on_copy_after = x; }
  static void on_move_after(int x) { throw_on_move_after = x; }

  static void do_not_throw()
  {
    throw_on_ctor_after = -1;
    throw_on_copy_after = -1;
    throw_on_move_after = -1;
  }

  static void in_constructor() { maybe_throw(throw_on_ctor_after); }
  static void in_copy() { maybe_throw(throw_on_copy_after); }
  static void in_move() { maybe_throw(throw_on_move_after); }

private:
  static void maybe_throw(int& counter)
  {
    if (counter > 0)
    {
      --counter;
      if (counter == 0)
      {
        --counter;
        throw test_exception();
      }
    }
  }
};

int test_elem_throw::throw_on_ctor_after = -1;
int test_elem_throw::throw_on_copy_after = -1;
int test_elem_throw::throw_on_move_after = -1;

struct test_elem_base
{
  test_elem_base()
  {
    test_elem_throw::in_constructor();
    _index = new int(0);
    ++_live_count;
  }

  test_elem_base(int index)
  {
    test_elem_throw::in_constructor();
    _index = new int(index);
    ++_live_count;
  }

  explicit test_elem_base(const test_elem_base& rhs)
  {
    test_elem_throw::in_copy();
    _index = new int(*rhs._index);
    ++_live_count;
  }

  test_elem_base(test_elem_base&& rhs)
  {
    test_elem_throw::in_move();
    _index = rhs._index;
    rhs._index = nullptr;
    ++_live_count;
  }

  void operator=(const test_elem_base& rhs)
  {
    test_elem_throw::in_copy();
    if (_index) { delete _index; }
    _index = new int(*rhs._index);
  }

  void operator=(test_elem_base&& rhs)
  {
    test_elem_throw::in_move();
    if (_index) { delete _index; }
    _index = rhs._index;
    rhs._index = nullptr;
  }

  ~test_elem_base()
  {
    if (_index) { delete _index; }
    --_live_count;
  }

  friend bool operator==(const test_elem_base& a, const test_elem_base& b)
  {
    return a._index && b._index && *(a._index) == *(b._index);
  }

  friend bool operator<(const test_elem_base& a, const test_elem_base& b)
  {
    return boost::less_pointees(a._index, b._index);
  }

  friend std::ostream& operator<<(std::ostream& out, const test_elem_base& elem)
  {
    if (elem._index) { out << *elem._index; }
    else { out << "null"; }
    return out;
  }

  static bool no_living_elem()
  {
    return _live_count == 0;
  }

private:
  int* _index;

  static int _live_count;
};

int test_elem_base::_live_count = 0;

struct regular_elem : test_elem_base
{
  regular_elem() = default;

  regular_elem(int index) : test_elem_base(index) {}

  regular_elem(const regular_elem& rhs)
    :test_elem_base(rhs)
  {}

  regular_elem(regular_elem&& rhs)
    :test_elem_base(std::move(rhs))
  {}

  void operator=(const regular_elem& rhs)
  {
    static_cast<test_elem_base&>(*this) = rhs;
  }

  void operator=(regular_elem&& rhs)
  {
    static_cast<test_elem_base&>(*this) = std::move(rhs);
  }
};

struct noex_move : test_elem_base
{
  noex_move() = default;

  noex_move(int index) : test_elem_base(index) {}

  noex_move(const noex_move& rhs)
    :test_elem_base(rhs)
  {}

  noex_move(noex_move&& rhs) noexcept
    :test_elem_base(std::move(rhs))
  {}

  void operator=(const noex_move& rhs)
  {
    static_cast<test_elem_base&>(*this) = rhs;
  }

  void operator=(noex_move&& rhs) noexcept
  {
    static_cast<test_elem_base&>(*this) = std::move(rhs);
  }
};

struct noex_copy : test_elem_base
{
  noex_copy() = default;

  noex_copy(int index) : test_elem_base(index) {}

  noex_copy(const noex_copy& rhs) noexcept
    :test_elem_base(rhs)
  {}

  noex_copy(noex_copy&& rhs)
    :test_elem_base(std::move(rhs))
  {}

  void operator=(const noex_copy& rhs) noexcept
  {
    static_cast<test_elem_base&>(*this) = rhs;
  }

  void operator=(noex_copy&& rhs)
  {
    static_cast<test_elem_base&>(*this) = std::move(rhs);
  }
};

struct only_movable : test_elem_base
{
  only_movable() = default;

  only_movable(int index) : test_elem_base(index) {}

  only_movable(only_movable&& rhs)
    :test_elem_base(std::move(rhs))
  {}

  void operator=(only_movable&& rhs)
  {
    static_cast<test_elem_base&>(*this) = std::move(rhs);
  }
};

struct no_default_ctor : test_elem_base
{
  no_default_ctor(int index) : test_elem_base(index) {}

  no_default_ctor(const no_default_ctor& rhs)
    :test_elem_base(rhs)
  {}

  no_default_ctor(no_default_ctor&& rhs)
    :test_elem_base(std::move(rhs))
  {}

  void operator=(const no_default_ctor& rhs)
  {
    static_cast<test_elem_base&>(*this) = rhs;
  }

  void operator=(no_default_ctor&& rhs)
  {
    static_cast<test_elem_base&>(*this) = std::move(rhs);
  }
};

template <typename Container>
class input_iterator : public std::iterator<std::input_iterator_tag, typename Container::value_type>
{
  typedef typename Container::iterator iterator;
  typedef typename Container::value_type value_type;

  struct erase_on_destroy {};

public:
  input_iterator(Container& c, iterator it)
    :_container(c),
     _it(it)
  {}

  input_iterator(const input_iterator& rhs)
    :_container(rhs._container),
     _it(rhs._it),
     _erase_on_destroy(rhs._erase_on_destroy)
  {
    rhs._erase_on_destroy = false;
  }

  input_iterator(const input_iterator& rhs, erase_on_destroy)
    :_container(rhs._container),
     _it(rhs._it),
     _erase_on_destroy(true)
  {}

  ~input_iterator()
  {
    if (_erase_on_destroy)
    {
      _container.erase(_it); // must not invalidate other iterators
    }
  }

  const value_type& operator*()
  {
    return *_it;
  }

  input_iterator operator++()
  {
    _container.erase(_it);
    ++_it;
    return *this;
  }

  input_iterator operator++(int)
  {
    input_iterator old(*this, erase_on_destroy{});
    ++_it;
    return old;
  }

  friend bool operator==(const input_iterator a, const input_iterator b)
  {
    return a._it == b._it;
  }

  friend bool operator!=(const input_iterator a, const input_iterator b)
  {
    return !(a == b);
  }

private:
  Container& _container;
  iterator _it;
  mutable bool _erase_on_destroy = false;
};

template <typename Container>
input_iterator<Container> make_input_iterator(Container& c, typename Container::iterator it)
{
  return input_iterator<Container>(c, it);
}

template <typename Container, typename T>
Container getRange(int count)
{
  Container c;
  c.reserve(count);

  for (int i = 1; i <= count; ++i)
  {
    c.push_back(T(i));
  }

  return c;
}

template <typename Devector, typename T>
Devector getRange(int fbeg, int fend, int bbeg, int bend)
{
  Devector c;

  c.reserve_front(fend - fbeg);
  c.reserve_back(bend - bbeg);

  for (int i = fend; i > fbeg ;)
  {
    c.emplace_front(--i);
  }

  for (int i = bbeg; i < bend; ++i)
  {
    c.emplace_back(i);
  }

  return c;
}

template <typename Range>
void printRange(std::ostream& out, const Range& range)
{
  out << '[';
  bool first = true;
  for (auto&& elem : range)
  {
    if (first) { first = false; }
    else { out << ','; }

    out << elem;
  }
  out << ']';
}

template <class T, class Allocator, class SBP, class GP>
std::ostream& operator<<(std::ostream& out, const devector<T, Allocator, SBP, GP>& devec)
{
  printRange(out, devec);
  return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& vec)
{
  printRange(out, vec);
  return out;
}

template <typename Devector>
void assert_equals(const Devector& actual, const std::vector<typename Devector::value_type>& expected)
{
  bool equals = boost::algorithm::equal(actual.begin(), actual.end(), expected.begin(), expected.end());

  if (!equals)
  {
    std::cerr << actual << " != " << expected << " (actual != expected)" << std::endl;

    BOOST_ASSERT(false);
  }
}

template <typename Devector>
void assert_equals(const Devector& actual, std::initializer_list<unsigned> inits)
{
  using T = typename Devector::value_type;
  std::vector<T> expected;
  for (unsigned init : inits)
  {
    expected.push_back(T(init));
  }

  assert_equals(actual, expected);
}

template <typename>
struct small_buffer_size;

template <typename U, typename A, typename SBP, typename GP>
struct small_buffer_size<devector<U, A, SBP, GP>>
{
  static const unsigned front_size = SBP::front_size;
  static const unsigned back_size = SBP::back_size;
  static const unsigned value = front_size + back_size;
};

// END HELPERS

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_default()
{
  Devector a;

  BOOST_ASSERT(a.empty());
  BOOST_ASSERT(a.capacity_alloc_count == 0);
  BOOST_ASSERT(a.capacity() == small_buffer_size<Devector>::value);
}


template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_allocator()
{
  typename Devector::allocator_type alloc_template;

  Devector a(alloc_template);

  BOOST_ASSERT(a.empty());
  BOOST_ASSERT(a.capacity_alloc_count == 0);
  BOOST_ASSERT(a.capacity() == small_buffer_size<Devector>::value);
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_reserve_only()
{
  {
    Devector a(16, reserve_only_tag{});
    BOOST_ASSERT(a.size() == 0);
    BOOST_ASSERT(a.capacity() >= 16);
  }

  {
    Devector b(0, reserve_only_tag{});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_n()
{
  {
    Devector a(8);

    assert_equals(a, {0, 0, 0, 0, 0, 0, 0, 0});
  }

  {
    Devector b(0);

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    test_elem_throw::on_ctor_after(4);

    try
    {
      Devector a(8);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}

    BOOST_ASSERT(test_elem_base::no_living_elem());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_n_copy()
{
  {
    const T x(9);
    Devector a(8, x);

    assert_equals(a, {9, 9, 9, 9, 9, 9, 9, 9});
  }

  {
    const T x(9);
    Devector b(0, x);

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    test_elem_throw::on_copy_after(4);

    try
    {
      const T x(404);
      Devector a(8, x);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}

    BOOST_ASSERT(test_elem_base::no_living_elem());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_input_range()
{
  {
    const devector<T> expected = getRange<devector<T>, T>(16);
    devector<T> input = expected;

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a(input_begin, input_end);
    BOOST_ASSERT(a == expected);
  }

  { // empty range
    devector<T> input;
    auto input_begin = make_input_iterator(input, input.begin());

    Devector b(input_begin, input_begin);

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  BOOST_ASSERT(test_elem_base::no_living_elem());

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    devector<T> input = getRange<devector<T>, T>(16);

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    test_elem_throw::on_copy_after(4);

    try
    {
      Devector c(input_begin, input_end);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}
  }

  BOOST_ASSERT(test_elem_base::no_living_elem());
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_forward_range()
{
  const std::forward_list<T> x{1, 2, 3, 4, 5, 6, 7, 8};

  {
    Devector a(x.begin(), x.end());

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(a.capacity_alloc_count <= 1);
  }

  {
    Devector b(x.begin(), x.begin());

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    test_elem_throw::on_copy_after(4);

    try
    {
      Devector c(x.begin(), x.end());
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_pointer_range()
{
  const std::vector<T> x = getRange<std::vector<T>, T>(8);
  const T* xbeg = x.data();
  const T* xend = x.data() + x.size();

  {
    Devector a(xbeg, xend);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(a.capacity_alloc_count <= 1);
  }

  {
    Devector b(xbeg, xbeg);

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    test_elem_throw::on_copy_after(4);

    try
    {
      Devector c(xbeg, xend);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_copy_constructor()
{
  {
    Devector a;
    Devector b(a);

    assert_equals(b, {});
    BOOST_ASSERT(b.capacity_alloc_count == 0);
  }

  {
    Devector a = getRange<Devector, T>(8);
    Devector b(a);

    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(b.capacity_alloc_count <= 1);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector a = getRange<Devector, T>(8);

    test_elem_throw::on_copy_after(4);

    try
    {
      Devector b(a);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_move_constructor()
{
  { // empty
    Devector a;
    Devector b(std::move(a));

    BOOST_ASSERT(a.empty());
    BOOST_ASSERT(b.empty());
  }

  { // maybe small
    Devector a = getRange<Devector, T>(1, 5, 5, 9);
    Devector b(std::move(a));

    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8});

    // a is unspecified but valid state
    a.clear();
    BOOST_ASSERT(a.empty());
  }

  { // big
    Devector a = getRange<Devector, T>(32);
    Devector b(std::move(a));

    std::vector<T> exp = getRange<std::vector<T>, T>(32);
    assert_equals(b, exp);

    // a is unspecified but valid state
    a.clear();
    BOOST_ASSERT(a.empty());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_destructor()
{
  Devector a;

  Devector b = getRange<Devector, T>(3);
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assignment()
{
  { // assign to empty (maybe small)
    Devector a;
    const Devector b = getRange<Devector, T>(6);

    a = b;

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);
    const Devector b;

    a = b;

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    const Devector b = getRange<Devector, T>(6);

    a = b;

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    const Devector b = getRange<Devector, T>(6);

    a = b;

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    const Devector b = getRange<Devector, T>(6);

    a = b;

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    const Devector b = getRange<Devector, T>(12);

    a = b;

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);
    const Devector b = getRange<Devector, T>(12);

    test_elem_throw::on_copy_after(3);

    try
    {
      a = b;
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_move_assignment()
{
  { // assign to empty (maybe small)
    Devector a;
    Devector b = getRange<Devector, T>(6);

    a = std::move(b);

    assert_equals(a, {1, 2, 3, 4, 5, 6});

    // b is in unspecified but valid state
    b.clear();
    assert_equals(b, {});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);
    Devector b;

    a = std::move(b);

    assert_equals(a, {});
    assert_equals(b, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    Devector b = getRange<Devector, T>(6);

    a = std::move(b);

    assert_equals(a, {1, 2, 3, 4, 5, 6});

    b.clear();
    assert_equals(b, {});
  }

  // move should be used on the slow path
  if (std::is_nothrow_move_constructible<T>::value)
  {
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    Devector b = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    a = std::move(b);

    test_elem_throw::do_not_throw();

    assert_equals(a, {1, 2, 3, 4, 5, 6});

    b.clear();
    assert_equals(b, {});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_il_assignment()
{
  { // assign to empty (maybe small)
    Devector a;

    a = {1, 2, 3, 4, 5, 6};

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);

    a = {};

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);

    a = {1, 2, 3, 4, 5, 6};

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a = {1, 2, 3, 4, 5, 6};

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a = {1, 2, 3, 4, 5, 6};

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assign_input_range()
{
  { // assign to empty, keep it small
    const devector<T> expected = getRange<devector<T>, T>(small_buffer_size<Devector>::value);
    devector<T> input = expected;

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a;

    a.assign(input_begin, input_end);

    BOOST_ASSERT(a == expected);
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assign to empty (maybe small)
    devector<T> input = getRange<devector<T>, T>(6);

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a;

    a.assign(input_begin, input_end);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    devector<T> input = getRange<devector<T>, T>(6);
    auto input_begin = make_input_iterator(input, input.begin());

    Devector a = getRange<Devector, T>(6);
    a.assign(input_begin, input_begin);

    assert_equals(a, {});
  }

  { // assign to non-empty
    devector<T> input = getRange<devector<T>, T>(6);
    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.assign(input_begin, input_end);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    devector<T> input = getRange<devector<T>, T>(6);
    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign(input_begin, input_end);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    devector<T> input = getRange<devector<T>, T>(6);
    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign(input_begin, input_end);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    devector<T> input = getRange<devector<T>, T>(12);
    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign(input_begin, input_end);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)

    devector<T> input = getRange<devector<T>, T>(12);
    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a.assign(input_begin, input_end);
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assign_forward_range()
{
  const std::forward_list<T> x{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  auto one = x.begin();
  auto six = one;
  auto twelve = one;

  std::advance(six, 6);
  std::advance(twelve, 12);

  { // assign to empty (maybe small)
    Devector a;

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);

    a.assign(one, one);

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign(one, twelve);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a.assign(one, twelve);
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assign_pointer_range()
{
  const std::vector<T> x = getRange<std::vector<T>, T>(12);
  const T* one = x.data();
  const T* six = one + 6;
  const T* twelve = one + 12;

  { // assign to empty (maybe small)
    Devector a;

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);

    a.assign(one, one);

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign(one, six);

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign(one, twelve);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a.assign(one, twelve);
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assign_n()
{
  { // assign to empty (maybe small)
    Devector a;

    a.assign(6, T(9));

    assert_equals(a, {9, 9, 9, 9, 9, 9});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);

    a.assign(0, T(404));

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);

    a.assign(6, T(9));

    assert_equals(a, {9, 9, 9, 9, 9, 9});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign(6, T(9));

    assert_equals(a, {9, 9, 9, 9, 9, 9});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign(6, T(9));

    assert_equals(a, {9, 9, 9, 9, 9, 9});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign(12, T(9));

    assert_equals(a, {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a.assign(12, T(9));
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_assign_il()
{
  { // assign to empty (maybe small)
    Devector a;

    a.assign({1, 2, 3, 4, 5, 6});

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign from empty
    Devector a = getRange<Devector, T>(6);

    a.assign({});

    assert_equals(a, {});
  }

  { // assign to non-empty
    Devector a = getRange<Devector, T>(11, 15, 15, 19);

    a.assign({1, 2, 3, 4, 5, 6});

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  { // assign to free front
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign({1, 2, 3, 4, 5, 6});

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment overlaps contents
    Devector a = getRange<Devector, T>(11, 15, 15, 19);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign({1, 2, 3, 4, 5, 6});

    assert_equals(a, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  { // assignment exceeds contents
    Devector a = getRange<Devector, T>(11, 13, 13, 15);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
    BOOST_ASSERT(a.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // strong guarantee if reallocation is needed (no guarantee otherwise)
    Devector a = getRange<Devector, T>(6);

    test_elem_throw::on_copy_after(3);

    try
    {
      a.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
      BOOST_ASSERT(false);
    } catch(const test_exception&) {}

    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_begin_end()
{
  std::vector<T> expected = getRange<std::vector<T>, T>(10);
  Devector actual = getRange<Devector, T>(10);

  BOOST_ASSERT(boost::algorithm::equal(expected.begin(), expected.end(), actual.begin(), actual.end()));
  BOOST_ASSERT(boost::algorithm::equal(expected.rbegin(), expected.rend(), actual.rbegin(), actual.rend()));
  BOOST_ASSERT(boost::algorithm::equal(expected.cbegin(), expected.cend(), actual.cbegin(), actual.cend()));
  BOOST_ASSERT(boost::algorithm::equal(expected.crbegin(), expected.crend(), actual.crbegin(), actual.crend()));

  const Devector cactual = getRange<Devector, T>(10);

  BOOST_ASSERT(boost::algorithm::equal(expected.begin(), expected.end(), cactual.begin(), cactual.end()));
  BOOST_ASSERT(boost::algorithm::equal(expected.rbegin(), expected.rend(), cactual.rbegin(), cactual.rend()));
}

template <typename Devector, typename T = typename Devector::value_type>
void test_empty()
{
  Devector a;
  BOOST_ASSERT(a.empty());

  a.push_front(T(1));
  BOOST_ASSERT(! a.empty());

  a.pop_back();
  BOOST_ASSERT(a.empty());

  Devector b(16, reserve_only_tag{});
  BOOST_ASSERT(b.empty());

  Devector c = getRange<Devector, T>(3);
  BOOST_ASSERT(! c.empty());
}

template <typename Devector, typename T = typename Devector::value_type>
void test_size()
{
  Devector a;
  BOOST_ASSERT(a.size() == 0);

  a.push_front(T(1));
  BOOST_ASSERT(a.size() == 1);

  a.pop_back();
  BOOST_ASSERT(a.size() == 0);

  Devector b(16, reserve_only_tag{});
  BOOST_ASSERT(b.size() == 0);

  Devector c = getRange<Devector, T>(3);
  BOOST_ASSERT(c.size() == 3);
}

template <typename Devector, typename T = typename Devector::value_type>
void test_capacity()
{
  Devector a;
  BOOST_ASSERT(a.capacity() == small_buffer_size<Devector>::value);

  Devector b(128, reserve_only_tag{});
  BOOST_ASSERT(b.capacity() >= 128);

  Devector c = getRange<Devector, T>(10);
  BOOST_ASSERT(c.capacity() >= 10);
}

template <typename Devector, typename T = typename Devector::value_type>
void test_resize_front()
{
  // size < required, alloc needed
  {
    Devector a = getRange<Devector, T>(5);
    a.resize_front(8);
    assert_equals(a, {0, 0, 0, 1, 2, 3, 4, 5});
  }

  // size < required, but capacity provided
  {
    Devector b = getRange<Devector, T>(5);
    b.reserve_front(16);
    b.resize_front(8);
    assert_equals(b, {0, 0, 0, 1, 2, 3, 4, 5});
  }

  // size < required, move would throw
  if (! std::is_nothrow_move_constructible<T>::value && std::is_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    test_elem_throw::on_move_after(3);

    c.resize_front(8); // shouldn't use the throwing move

    test_elem_throw::do_not_throw();
    assert_equals(c, {0, 0, 0, 1, 2, 3, 4, 5});
  }

  // size < required, constructor throws
  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector d = getRange<Devector, T>(5);
    std::vector<T> d_origi = getRange<std::vector<T>, T>(5);
    auto origi_begin = d.begin();
    test_elem_throw::on_ctor_after(3);

    try
    {
      d.resize_front(256);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(d, d_origi);
    BOOST_ASSERT(origi_begin == d.begin());
  }

  // size >= required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_front(4);
    assert_equals(e, {3, 4, 5, 6});
  }

  // size < required, does not fit front small buffer
  {
    std::vector<T> expected(128);
    Devector g;
    g.resize_front(128);
    assert_equals(g, expected);
  }

  // size = required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_front(6);
    assert_equals(e, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_resize_front_copy()
{
  // size < required, alloc needed
  {
    Devector a = getRange<Devector, T>(5);
    a.resize_front(8, T(9));
    assert_equals(a, {9, 9, 9, 1, 2, 3, 4, 5});
  }

  // size < required, but capacity provided
  {
    Devector b = getRange<Devector, T>(5);
    b.reserve_front(16);
    b.resize_front(8, T(9));
    assert_equals(b, {9, 9, 9, 1, 2, 3, 4, 5});
  }

  // size < required, copy throws
  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    std::vector<T> c_origi = getRange<std::vector<T>, T>(5);
    test_elem_throw::on_copy_after(3);

    try
    {
      c.resize_front(256, T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(c, c_origi);
  }

  // size < required, copy throws, but later
  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    std::vector<T> c_origi = getRange<std::vector<T>, T>(5);
    auto origi_begin = c.begin();
    test_elem_throw::on_copy_after(7);

    try
    {
      c.resize_front(256, T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(c, c_origi);
    BOOST_ASSERT(origi_begin == c.begin());
  }

  // size >= required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_front(4, T(404));
    assert_equals(e, {3, 4, 5, 6});
  }

  // size < required, does not fit front small buffer
  {
    std::vector<T> expected(128, T(9));
    Devector g;
    g.resize_front(128, T(9));
    assert_equals(g, expected);
  }

  // size = required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_front(6, T(9));
    assert_equals(e, {1, 2, 3, 4, 5, 6});
  }

  // size < required, tmp is already inserted
  {
    Devector f = getRange<Devector, T>(8);
    const T& tmp = *(f.begin() + 1);
    f.resize_front(16, tmp);
    assert_equals(f, {2,2,2,2,2,2,2,2,1,2,3,4,5,6,7,8});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_resize_back()
{
  // size < required, alloc needed
  {
    Devector a = getRange<Devector, T>(5);
    a.resize_back(8);
    assert_equals(a, {1, 2, 3, 4, 5, 0, 0, 0});
  }

  // size < required, but capacity provided
  {
    Devector b = getRange<Devector, T>(5);
    b.reserve_back(16);
    b.resize_back(8);
    assert_equals(b, {1, 2, 3, 4, 5, 0, 0, 0});
  }

  // size < required, move would throw
  if (! std::is_nothrow_move_constructible<T>::value && std::is_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    test_elem_throw::on_move_after(3);

    c.resize_back(8); // shouldn't use the throwing move

    test_elem_throw::do_not_throw();
    assert_equals(c, {1, 2, 3, 4, 5, 0, 0, 0});
  }

  // size < required, constructor throws
  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector d = getRange<Devector, T>(5);
    std::vector<T> d_origi = getRange<std::vector<T>, T>(5);
    auto origi_begin = d.begin();
    test_elem_throw::on_ctor_after(3);

    try
    {
      d.resize_back(256);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(d, d_origi);
    BOOST_ASSERT(origi_begin == d.begin());
  }

  // size >= required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_back(4);
    assert_equals(e, {1, 2, 3, 4});
  }

  // size < required, does not fit front small buffer
  {
    std::vector<T> expected(128);
    Devector g;
    g.resize_back(128);
    assert_equals(g, expected);
  }

  // size = required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_back(6);
    assert_equals(e, {1, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_resize_back_copy()
{
  // size < required, alloc needed
  {
    Devector a = getRange<Devector, T>(5);
    a.resize_back(8, T(9));
    assert_equals(a, {1, 2, 3, 4, 5, 9, 9, 9});
  }

  // size < required, but capacity provided
  {
    Devector b = getRange<Devector, T>(5);
    b.reserve_back(16);
    b.resize_back(8, T(9));
    assert_equals(b, {1, 2, 3, 4, 5, 9, 9, 9});
  }

  // size < required, copy throws
  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    std::vector<T> c_origi = getRange<std::vector<T>, T>(5);
    test_elem_throw::on_copy_after(3);

    try
    {
      c.resize_back(256, T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(c, c_origi);
  }

  // size < required, copy throws, but later
  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    std::vector<T> c_origi = getRange<std::vector<T>, T>(5);
    auto origi_begin = c.begin();
    test_elem_throw::on_copy_after(7);

    try
    {
      c.resize_back(256, T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(c, c_origi);
    BOOST_ASSERT(origi_begin == c.begin());
  }

  // size < required, copy throws
  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector c = getRange<Devector, T>(5);
    std::vector<T> c_origi = getRange<std::vector<T>, T>(5);
    auto origi_begin = c.begin();
    test_elem_throw::on_copy_after(3);

    try
    {
      c.resize_back(256, T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(c, c_origi);
    BOOST_ASSERT(origi_begin == c.begin());
  }

  // size >= required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_back(4, T(404));
    assert_equals(e, {1, 2, 3, 4});
  }

  // size < required, does not fit front small buffer
  {
    std::vector<T> expected(128, T(9));
    Devector g;
    g.resize_back(128, T(9));
    assert_equals(g, expected);
  }

  // size = required
  {
    Devector e = getRange<Devector, T>(6);
    e.resize_back(6, T(9));
    assert_equals(e, {1, 2, 3, 4, 5, 6});
  }

  // size < required, tmp is already inserted
  {
    Devector f = getRange<Devector, T>(8);
    const T& tmp = *(f.begin() + 1);
    f.resize_back(16, tmp);
    assert_equals(f, {1,2,3,4,5,6,7,8,2,2,2,2,2,2,2,2});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_reserve_front()
{
  Devector a;

  a.reserve_front(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    a.push_front(i);
  }

  BOOST_ASSERT(a.capacity_alloc_count == 1);

  Devector b;
  b.reserve_front(4);
  b.reserve_front(6);
  b.reserve_front(4);
  b.reserve_front(8);
  b.reserve_front(16);
}

template <typename Devector, typename T = typename Devector::value_type>
void test_reserve_back()
{
  Devector a;

  a.reserve_back(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    a.push_back(i);
  }

  BOOST_ASSERT(a.capacity_alloc_count == 1);

  Devector b;
  b.reserve_back(4);
  b.reserve_back(6);
  b.reserve_back(4);
  b.reserve_back(8);
  b.reserve_back(16);
}

template <typename Devector>
void test_shrink_to_fit_always()
{
  Devector a;
  a.reserve(100);

  a.push_back(1);
  a.push_back(2);
  a.push_back(3);

  a.shrink_to_fit();

  std::vector<unsigned> expected{1, 2, 3};
  assert_equals(a, expected);

  unsigned sb_size = small_buffer_size<Devector>::value;
  BOOST_ASSERT(a.capacity() == (std::max)(sb_size, 3u));
}

template <typename Devector>
void test_shrink_to_fit_never()
{
  Devector a;
  a.reserve(100);

  a.push_back(1);
  a.push_back(2);
  a.push_back(3);

  a.shrink_to_fit();

  std::vector<unsigned> expected{1, 2, 3};
  assert_equals(a, expected);
  BOOST_ASSERT(a.capacity() == 100);
}

void test_shrink_to_fit()
{
  struct always_shrink : public devector_growth_policy
  {
    static bool should_shrink(unsigned, unsigned, unsigned)
    {
      return true;
    }
  };

  struct never_shrink : public devector_growth_policy
  {
    static bool should_shrink(unsigned, unsigned, unsigned)
    {
      return false;
    }
  };

  using devector_u_shr       = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 0>, always_shrink>;
  using small_devector_u_shr = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 3>, always_shrink>;

  test_shrink_to_fit_always<devector_u_shr>();
  test_shrink_to_fit_always<small_devector_u_shr>();

  using devector_u           = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 0>, never_shrink>;
  using small_devector_u     = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 3>, never_shrink>;

  test_shrink_to_fit_never<devector_u>();
  test_shrink_to_fit_never<small_devector_u>();
}

template <typename Devector, typename T = typename Devector::value_type>
void test_index_operator()
{
  { // non-const []
    Devector a = getRange<Devector, T>(5);

    BOOST_ASSERT(a[0] == T(1));
    BOOST_ASSERT(a[4] == T(5));
    BOOST_ASSERT(&a[3] == &a[0] + 3);

    a[0] = T(100);
    BOOST_ASSERT(a[0] == T(100));
  }

  { // const []
    const Devector a = getRange<Devector, T>(5);

    BOOST_ASSERT(a[0] == T(1));
    BOOST_ASSERT(a[4] == T(5));
    BOOST_ASSERT(&a[3] == &a[0] + 3);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_at()
{
  { // non-const at
    Devector a = getRange<Devector, T>(3);

    BOOST_ASSERT(a.at(0) == T(1));
    a.at(0) = T(100);
    BOOST_ASSERT(a.at(0) == T(100));

    try
    {
      a.at(3);
      BOOST_ASSERT(false);
    } catch (const std::out_of_range&) {}
  }

  { // const at
    const Devector a = getRange<Devector, T>(3);

    BOOST_ASSERT(a.at(0) == T(1));

    try
    {
      a.at(3);
      BOOST_ASSERT(false);
    } catch (const std::out_of_range&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_front()
{
  { // non-const front
    Devector a = getRange<Devector, T>(3);
    BOOST_ASSERT(a.front() == T(1));
    a.front() = T(100);
    BOOST_ASSERT(a.front() == T(100));
  }

  { // const front
    const Devector a = getRange<Devector, T>(3);
    BOOST_ASSERT(a.front() == T(1));
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_back()
{
  { // non-const back
    Devector a = getRange<Devector, T>(3);
    BOOST_ASSERT(a.back() == T(3));
    a.back() = T(100);
    BOOST_ASSERT(a.back() == T(100));
  }

  { // const back
    const Devector a = getRange<Devector, T>(3);
    BOOST_ASSERT(a.back() == T(3));
  }
}

void test_data()
{
  unsigned c_array[] = {1, 2, 3, 4};

  { // non-const data
    devector<unsigned> a(c_array, c_array + 4);
    BOOST_ASSERT(a.data() == &a.front());

    BOOST_ASSERT(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);

    *(a.data()) = 100;
    BOOST_ASSERT(a.front() == 100);
  }

  { // const data
    const devector<unsigned> a(c_array, c_array + 4);
    BOOST_ASSERT(a.data() == &a.front());

    BOOST_ASSERT(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_emplace_front()
{
  {
    Devector a;

    a.emplace_front(3);
    a.emplace_front(2);
    a.emplace_front(1);

    std::vector<T> expected = getRange<std::vector<T>, T>(3);

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      test_elem_throw::on_ctor_after(1);
      b.emplace_front(404);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_push_front()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    std::reverse(expected.begin(), expected.end());
    Devector a;

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.push_front(elem);
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      T elem(404);
      test_elem_throw::on_copy_after(1);
      b.push_front(elem);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }

  // test when tmp is already inserted
  {
    Devector c = getRange<Devector, T>(4);
    const T& tmp = *(c.begin() + 1);
    c.push_front(tmp);
    assert_equals(c, {2, 1, 2, 3, 4});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_push_front_rvalue()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    std::reverse(expected.begin(), expected.end());
    Devector a;

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.push_front(std::move(elem));
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_move_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      test_elem_throw::on_move_after(1);
      b.push_front(T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_unsafe_push_front()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    std::reverse(expected.begin(), expected.end());
    Devector a;
    a.reserve_front(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_front(elem);
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    b.reserve_front(5);
    auto origi_begin = b.begin();

    try
    {
      T elem(404);
      test_elem_throw::on_copy_after(1);
      b.unsafe_push_front(elem);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_unsafe_push_front_rvalue()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    std::reverse(expected.begin(), expected.end());
    Devector a;
    a.reserve_front(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_front(std::move(elem));
    }

    assert_equals(a, expected);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_pop_front()
{
  {
    Devector a;
    a.emplace_front(1);
    a.pop_front();
    BOOST_ASSERT(a.empty());
  }

  {
    Devector b;

    b.emplace_back(2);
    b.pop_front();
    BOOST_ASSERT(b.empty());

    b.emplace_front(3);
    b.pop_front();
    BOOST_ASSERT(b.empty());
  }

  {
    Devector c = getRange<Devector, T>(20);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_ASSERT(!c.empty());
      c.pop_front();
    }
    BOOST_ASSERT(c.empty());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_emplace_back()
{
  {
    Devector a;

    a.emplace_back(1);
    a.emplace_back(2);
    a.emplace_back(3);

    std::vector<T> expected = getRange<std::vector<T>, T>(3);

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      test_elem_throw::on_ctor_after(1);
      b.emplace_back(404);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_push_back()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    Devector a;

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.push_back(elem);
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      T elem(404);
      test_elem_throw::on_copy_after(1);
      b.push_back(elem);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }

  // test when tmp is already inserted
  {
    Devector c = getRange<Devector, T>(4);
    const T& tmp = *(c.begin() + 1);
    c.push_back(tmp);
    assert_equals(c, {1, 2, 3, 4, 2});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_push_back_rvalue()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    Devector a;

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.push_back(std::move(elem));
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_move_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    auto origi_begin = b.begin();

    try
    {
      test_elem_throw::on_move_after(1);
      b.push_back(T(404));
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_unsafe_push_back()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    Devector a;
    a.reserve(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_back(elem);
    }

    assert_equals(a, expected);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector b = getRange<Devector, T>(4);
    b.reserve(5);
    auto origi_begin = b.begin();

    try
    {
      T elem(404);
      test_elem_throw::on_copy_after(1);
      b.unsafe_push_back(elem);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    auto new_begin = b.begin();

    BOOST_ASSERT(origi_begin == new_begin);
    BOOST_ASSERT(b.size() == 4);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_unsafe_push_back_rvalue()
{
  {
    std::vector<T> expected = getRange<std::vector<T>, T>(16);
    Devector a;
    a.reserve(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_back(std::move(elem));
    }

    assert_equals(a, expected);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_pop_back()
{
  {
    Devector a;
    a.emplace_back(1);
    a.pop_back();
    BOOST_ASSERT(a.empty());
  }

  {
    Devector b;

    b.emplace_front(2);
    b.pop_back();
    BOOST_ASSERT(b.empty());

    b.emplace_back(3);
    b.pop_back();
    BOOST_ASSERT(b.empty());
  }

  {
    Devector c = getRange<Devector, T>(20);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_ASSERT(!c.empty());
      c.pop_back();
    }
    BOOST_ASSERT(c.empty());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_emplace()
{
  {
    Devector a = getRange<Devector, T>(16);
    auto it = a.emplace(a.begin(), 123);
    assert_equals(a, {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector b = getRange<Devector, T>(16);
    auto it = b.emplace(b.end(), 123);
    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector c = getRange<Devector, T>(16);
    c.pop_front();
    auto it = c.emplace(c.begin(), 123);
    assert_equals(c, {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector d = getRange<Devector, T>(16);
    d.pop_back();
    auto it = d.emplace(d.end(), 123);
    assert_equals(d, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector e = getRange<Devector, T>(16);
    auto it = e.emplace(e.begin() + 5, 123);
    assert_equals(e, {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector f = getRange<Devector, T>(16);
    f.pop_front();
    f.pop_back();
    auto valid = f.begin() + 1;
    auto it = f.emplace(f.begin() + 1, 123);
    assert_equals(f, {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(3));
  }

  {
    Devector g = getRange<Devector, T>(16);
    g.pop_front();
    g.pop_back();
    auto valid = g.end() - 2;
    auto it = g.emplace(g.end() - 1, 123);
    assert_equals(g, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(14));
  }

  {
    Devector h = getRange<Devector, T>(16);
    h.pop_front();
    h.pop_back();
    auto valid = h.begin() + 7;
    auto it = h.emplace(h.begin() + 7, 123);
    assert_equals(h, {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(9));
  }

  {
    Devector i;
    i.emplace(i.begin(), 1);
    i.emplace(i.end(), 10);
    for (int j = 2; j < 10; ++j)
    {
      i.emplace(i.begin() + (j-1), j);
    }
    assert_equals(i, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector j = getRange<Devector, T>(4);
    auto origi_begin = j.begin();

    try
    {
      test_elem_throw::on_ctor_after(1);
      j.emplace(j.begin() + 2, 404);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(j, {1, 2, 3, 4});
    BOOST_ASSERT(origi_begin == j.begin());
  }

  // It's not required to pass the following test per C++11 23.3.6.5/1
  // If an exception is thrown other than by the copy constructor, move constructor,
  // assignment operator, or move assignment operator of T or by any InputIterator operation
  // there are no effects. If an exception is thrown by the move constructor of a non-CopyInsertable T,
  // the effects are unspecified.

//  if (! std::is_nothrow_move_constructible<T>::value && ! std::is_nothrow_copy_constructible<T>::value)
//  {
//    Devector k = getRange<Devector, T>(8);
//    auto origi_begin = k.begin();
//
//    try
//    {
//      test_elem_throw::on_copy_after(3);
//      test_elem_throw::on_move_after(3);
//      k.emplace(k.begin() + 4, 404);
//      BOOST_ASSERT(false);
//    }
//    catch (const test_exception&) {}
//
//    test_elem_throw::do_not_throw();
//
//    assert_equals(k, {1, 2, 3, 4, 5, 6, 7, 8});
//    BOOST_ASSERT(origi_begin == k.begin());
//  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_insert()
{
  T test_elem(123);

  {
    Devector a = getRange<Devector, T>(16);
    auto it = a.insert(a.begin(), test_elem);
    assert_equals(a, {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector b = getRange<Devector, T>(16);
    auto it = b.insert(b.end(), test_elem);
    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector c = getRange<Devector, T>(16);
    c.pop_front();
    auto it = c.insert(c.begin(), test_elem);
    assert_equals(c, {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector d = getRange<Devector, T>(16);
    d.pop_back();
    auto it = d.insert(d.end(), test_elem);
    assert_equals(d, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector e = getRange<Devector, T>(16);
    auto it = e.insert(e.begin() + 5, test_elem);
    assert_equals(e, {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector f = getRange<Devector, T>(16);
    f.pop_front();
    f.pop_back();
    auto valid = f.begin() + 1;
    auto it = f.insert(f.begin() + 1, test_elem);
    assert_equals(f, {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(3));
  }

  {
    Devector g = getRange<Devector, T>(16);
    g.pop_front();
    g.pop_back();
    auto valid = g.end() - 2;
    auto it = g.insert(g.end() - 1, test_elem);
    assert_equals(g, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(14));
  }

  {
    Devector h = getRange<Devector, T>(16);
    h.pop_front();
    h.pop_back();
    auto valid = h.begin() + 7;
    auto it = h.insert(h.begin() + 7, test_elem);
    assert_equals(h, {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(9));
  }

  {
    Devector i;
    i.insert(i.begin(), 1);
    i.insert(i.end(), 10);
    for (int j = 2; j < 10; ++j)
    {
      T x(j);
      i.insert(i.begin() + (j-1), x);
    }
    assert_equals(i, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Devector j = getRange<Devector, T>(4);
    auto origi_begin = j.begin();

    try
    {
      test_elem_throw::on_copy_after(1);
      j.insert(j.begin() + 2, test_elem);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(j, {1, 2, 3, 4});
    BOOST_ASSERT(origi_begin == j.begin());
  }

  // test when tmp is already inserted and there's free capacity
  {
    Devector c = getRange<Devector, T>(6);
    c.pop_back();
    const T& tmp = *(c.begin() + 2);
    c.insert(c.begin() + 1, tmp);
    assert_equals(c, {1, 3, 2, 3, 4, 5});
  }

  // test when tmp is already inserted and maybe there's no free capacity
  {
    Devector c = getRange<Devector, T>(6);
    const T& tmp = *(c.begin() + 2);
    c.insert(c.begin() + 1, tmp);
    assert_equals(c, {1, 3, 2, 3, 4, 5, 6});
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_insert_rvalue()
{
  {
    Devector a = getRange<Devector, T>(16);
    auto it = a.insert(a.begin(), 123);
    assert_equals(a, {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector b = getRange<Devector, T>(16);
    auto it = b.insert(b.end(), 123);
    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector c = getRange<Devector, T>(16);
    c.pop_front();
    auto it = c.insert(c.begin(), 123);
    assert_equals(c, {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector d = getRange<Devector, T>(16);
    d.pop_back();
    auto it = d.insert(d.end(), 123);
    assert_equals(d, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector e = getRange<Devector, T>(16);
    auto it = e.insert(e.begin() + 5, 123);
    assert_equals(e, {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    BOOST_ASSERT(*it == T(123));
  }

  {
    Devector f = getRange<Devector, T>(16);
    f.pop_front();
    f.pop_back();
    auto valid = f.begin() + 1;
    auto it = f.insert(f.begin() + 1, 123);
    assert_equals(f, {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(3));
  }

  {
    Devector g = getRange<Devector, T>(16);
    g.pop_front();
    g.pop_back();
    auto valid = g.end() - 2;
    auto it = g.insert(g.end() - 1, 123);
    assert_equals(g, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(14));
  }

  {
    Devector h = getRange<Devector, T>(16);
    h.pop_front();
    h.pop_back();
    auto valid = h.begin() + 7;
    auto it = h.insert(h.begin() + 7, 123);
    assert_equals(h, {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15});
    BOOST_ASSERT(*it == T(123));
    BOOST_ASSERT(*valid == T(9));
  }

  {
    Devector i;
    i.insert(i.begin(), 1);
    i.insert(i.end(), 10);
    for (int j = 2; j < 10; ++j)
    {
      i.insert(i.begin() + (j-1), j);
    }
    assert_equals(i, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Devector j = getRange<Devector, T>(4);
    auto origi_begin = j.begin();

    try
    {
      test_elem_throw::on_ctor_after(1);
      j.insert(j.begin() + 2, 404);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

    assert_equals(j, {1, 2, 3, 4});
    BOOST_ASSERT(origi_begin == j.begin());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_insert_n()
{
  {
    Devector a;
    const T x(123);
    auto ret = a.insert(a.end(), 5, x);
    assert_equals(a, {123, 123, 123, 123, 123});
    BOOST_ASSERT(ret == a.begin());
  }

  {
    Devector b = getRange<Devector, T>(8);
    const T x(9);
    auto ret = b.insert(b.begin(), 3, x);
    assert_equals(b, {9, 9, 9, 1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == b.begin());
  }

  {
    Devector c = getRange<Devector, T>(8);
    const T x(9);
    auto ret = c.insert(c.end(), 3, x);
    assert_equals(c, {1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9});
    BOOST_ASSERT(ret == c.begin() + 8);
  }

  {
    Devector d = getRange<Devector, T>(8);

    d.pop_front();
    d.pop_front();
    d.pop_front();

    const T x(9);
    auto origi_end = d.end();
    auto ret = d.insert(d.begin(), 3, x);

    assert_equals(d, {9, 9, 9, 4, 5, 6, 7, 8});
    BOOST_ASSERT(origi_end == d.end());
    BOOST_ASSERT(ret == d.begin());
  }

  {
    Devector e = getRange<Devector, T>(8);

    e.pop_back();
    e.pop_back();
    e.pop_back();

    const T x(9);
    auto origi_begin = e.begin();
    auto ret = e.insert(e.end(), 3, x);

    assert_equals(e, {1, 2, 3, 4, 5, 9, 9, 9});
    BOOST_ASSERT(origi_begin == e.begin());
    BOOST_ASSERT(ret == e.begin() + 5);
  }

  {
    Devector f = getRange<Devector, T>(8);
    f.reset_alloc_stats();

    f.pop_front();
    f.pop_front();
    f.pop_back();
    f.pop_back();

    const T x(9);
    auto ret = f.insert(f.begin() + 2, 4, x);

    assert_equals(f, {3, 4, 9, 9, 9, 9, 5, 6});
    BOOST_ASSERT(f.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == f.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);
    g.reset_alloc_stats();

    g.pop_front();
    g.pop_front();
    g.pop_back();
    g.pop_back();
    g.pop_back();

    const T x(9);
    auto ret = g.insert(g.begin() + 2, 5, x);

    assert_equals(g, {3, 4, 9, 9, 9, 9, 9, 5});
    BOOST_ASSERT(g.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);

    const T x(9);
    auto ret = g.insert(g.begin() + 2, 5, x);

    assert_equals(g, {1, 2, 9, 9, 9, 9, 9, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  { // n == 0
    Devector h = getRange<Devector, T>(8);
    h.reset_alloc_stats();

    const T x(9);

    auto ret = h.insert(h.begin(), 0, x);
    BOOST_ASSERT(ret == h.begin());

    ret = h.insert(h.begin() + 4, 0, x);
    BOOST_ASSERT(ret == h.begin() + 4);

    ret = h.insert(h.end(), 0, x);
    BOOST_ASSERT(ret == h.end());

    assert_equals(h, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(h.capacity_alloc_count == 0);
  }

  { // test insert already inserted
    Devector i = getRange<Devector, T>(8);
    i.reset_alloc_stats();

    i.pop_front();
    i.pop_front();

    auto ret = i.insert(i.end() - 1, 2, *i.begin());

    assert_equals(i, {3, 4, 5, 6, 7, 3, 3, 8});
    BOOST_ASSERT(i.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == i.begin() + 5);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // insert at begin
    try
    {
      Devector j = getRange<Devector, T>(4);

      const T x(404);
      test_elem_throw::on_copy_after(3);

      j.insert(j.begin(), 4, x);
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

    // insert at end
    try
    {
      Devector k = getRange<Devector, T>(4);

      const T x(404);
      test_elem_throw::on_copy_after(3);

      k.insert(k.end(), 4, x);
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_insert_range()
{
  std::vector<T> x{T(9), T(9), T(9), T(9), T(9)};
  auto xb = x.begin();

  {
    Devector a;
    auto ret = a.insert(a.end(), xb, xb+5);
    assert_equals(a, {9, 9, 9, 9, 9});
    BOOST_ASSERT(ret == a.begin());
  }

  {
    Devector b = getRange<Devector, T>(8);
    auto ret = b.insert(b.begin(), xb, xb+3);
    assert_equals(b, {9, 9, 9, 1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == b.begin());
  }

  {
    Devector c = getRange<Devector, T>(8);
    auto ret = c.insert(c.end(), xb, xb+3);
    assert_equals(c, {1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9});
    BOOST_ASSERT(ret == c.begin() + 8);
  }

  {
    Devector d = getRange<Devector, T>(8);

    d.pop_front();
    d.pop_front();
    d.pop_front();

    auto origi_end = d.end();
    auto ret = d.insert(d.begin(), xb, xb+3);

    assert_equals(d, {9, 9, 9, 4, 5, 6, 7, 8});
    BOOST_ASSERT(origi_end == d.end());
    BOOST_ASSERT(ret == d.begin());
  }

  {
    Devector e = getRange<Devector, T>(8);

    e.pop_back();
    e.pop_back();
    e.pop_back();

    auto origi_begin = e.begin();
    auto ret = e.insert(e.end(), xb, xb+3);

    assert_equals(e, {1, 2, 3, 4, 5, 9, 9, 9});
    BOOST_ASSERT(origi_begin == e.begin());
    BOOST_ASSERT(ret == e.begin() + 5);
  }

  {
    Devector f = getRange<Devector, T>(8);
    f.reset_alloc_stats();

    f.pop_front();
    f.pop_front();
    f.pop_back();
    f.pop_back();

    auto ret = f.insert(f.begin() + 2, xb, xb+4);

    assert_equals(f, {3, 4, 9, 9, 9, 9, 5, 6});
    BOOST_ASSERT(f.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == f.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);
    g.reset_alloc_stats();

    g.pop_front();
    g.pop_front();
    g.pop_back();
    g.pop_back();
    g.pop_back();

    auto ret = g.insert(g.begin() + 2, xb, xb+5);

    assert_equals(g, {3, 4, 9, 9, 9, 9, 9, 5});
    BOOST_ASSERT(g.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);

    auto ret = g.insert(g.begin() + 2, xb, xb+5);

    assert_equals(g, {1, 2, 9, 9, 9, 9, 9, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  { // n == 0
    Devector h = getRange<Devector, T>(8);
    h.reset_alloc_stats();

    auto ret = h.insert(h.begin(), xb, xb);
    BOOST_ASSERT(ret == h.begin());

    ret = h.insert(h.begin() + 4, xb, xb);
    BOOST_ASSERT(ret == h.begin() + 4);

    ret = h.insert(h.end(), xb, xb);
    BOOST_ASSERT(ret == h.end());

    assert_equals(h, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(h.capacity_alloc_count == 0);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // insert at begin
    try
    {
      Devector j = getRange<Devector, T>(4);

      test_elem_throw::on_copy_after(3);

      j.insert(j.begin(), xb, xb+4);
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

    // insert at end
    try
    {
      Devector k = getRange<Devector, T>(4);

      test_elem_throw::on_copy_after(3);

      k.insert(k.end(), xb, xb+4);
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_insert_init_list()
{
  {
    Devector a;
    auto ret = a.insert(a.end(), {T(123), T(123), T(123), T(123), T(123)});
    assert_equals(a, {123, 123, 123, 123, 123});
    BOOST_ASSERT(ret == a.begin());
  }

  {
    Devector b = getRange<Devector, T>(8);
    auto ret = b.insert(b.begin(), {T(9), T(9), T(9)});
    assert_equals(b, {9, 9, 9, 1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == b.begin());
  }

  {
    Devector c = getRange<Devector, T>(8);
    auto ret = c.insert(c.end(), {T(9), T(9), T(9)});
    assert_equals(c, {1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9});
    BOOST_ASSERT(ret == c.begin() + 8);
  }

  {
    Devector d = getRange<Devector, T>(8);

    d.pop_front();
    d.pop_front();
    d.pop_front();

    auto origi_end = d.end();
    auto ret = d.insert(d.begin(), {T(9), T(9), T(9)});

    assert_equals(d, {9, 9, 9, 4, 5, 6, 7, 8});
    BOOST_ASSERT(origi_end == d.end());
    BOOST_ASSERT(ret == d.begin());
  }

  {
    Devector e = getRange<Devector, T>(8);

    e.pop_back();
    e.pop_back();
    e.pop_back();

    auto origi_begin = e.begin();
    auto ret = e.insert(e.end(), {T(9), T(9), T(9)});

    assert_equals(e, {1, 2, 3, 4, 5, 9, 9, 9});
    BOOST_ASSERT(origi_begin == e.begin());
    BOOST_ASSERT(ret == e.begin() + 5);
  }

  {
    Devector f = getRange<Devector, T>(8);
    f.reset_alloc_stats();

    f.pop_front();
    f.pop_front();
    f.pop_back();
    f.pop_back();

    auto ret = f.insert(f.begin() + 2, {T(9), T(9), T(9), T(9)});

    assert_equals(f, {3, 4, 9, 9, 9, 9, 5, 6});
    BOOST_ASSERT(f.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == f.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);
    g.reset_alloc_stats();

    g.pop_front();
    g.pop_front();
    g.pop_back();
    g.pop_back();
    g.pop_back();

    auto ret = g.insert(g.begin() + 2, {T(9), T(9), T(9), T(9), T(9)});

    assert_equals(g, {3, 4, 9, 9, 9, 9, 9, 5});
    BOOST_ASSERT(g.capacity_alloc_count == 0);
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  {
    Devector g = getRange<Devector, T>(8);

    auto ret = g.insert(g.begin() + 2, {T(9), T(9), T(9), T(9), T(9)});

    assert_equals(g, {1, 2, 9, 9, 9, 9, 9, 3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == g.begin() + 2);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    // insert at begin
    try
    {
      Devector j = getRange<Devector, T>(4);

      test_elem_throw::on_copy_after(3);

      j.insert(j.begin(), {T(9), T(9), T(9), T(9), T(9)});
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

    // insert at end
    try
    {
      Devector k = getRange<Devector, T>(4);

      test_elem_throw::on_copy_after(3);

      k.insert(k.end(), {T(9), T(9), T(9), T(9), T(9)});
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_erase()
{
  {
    Devector a = getRange<Devector, T>(4);
    auto ret = a.erase(a.begin());
    assert_equals(a, {2, 3, 4});
    BOOST_ASSERT(ret == a.begin());
  }

  {
    Devector b = getRange<Devector, T>(4);
    auto ret = b.erase(b.end() - 1);
    assert_equals(b, {1, 2, 3});
    BOOST_ASSERT(ret == b.end());
  }

  {
    Devector c = getRange<Devector, T>(6);
    auto ret = c.erase(c.begin() + 2);
    assert_equals(c, {1, 2, 4, 5, 6});
    BOOST_ASSERT(ret == c.begin() + 2);
    BOOST_ASSERT(c.front_free_capacity() > 0);
  }

  {
    Devector d = getRange<Devector, T>(6);
    auto ret = d.erase(d.begin() + 4);
    assert_equals(d, {1, 2, 3, 4, 6});
    BOOST_ASSERT(ret == d.begin() + 4);
    BOOST_ASSERT(d.back_free_capacity() > 0);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_erase_range()
{
  {
    Devector a = getRange<Devector, T>(4);
    a.erase(a.end(), a.end());
    a.erase(a.begin(), a.begin());
  }

  {
    Devector b = getRange<Devector, T>(8);
    auto ret = b.erase(b.begin(), b.begin() + 2);
    assert_equals(b, {3, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == b.begin());
    BOOST_ASSERT(b.front_free_capacity() > 0);
  }

  {
    Devector c = getRange<Devector, T>(8);
    auto ret = c.erase(c.begin() + 1, c.begin() + 3);
    assert_equals(c, {1, 4, 5, 6, 7, 8});
    BOOST_ASSERT(ret == c.begin() + 1);
    BOOST_ASSERT(c.front_free_capacity() > 0);
  }

  {
    Devector d = getRange<Devector, T>(8);
    auto ret = d.erase(d.end() - 2, d.end());
    assert_equals(d, {1, 2, 3, 4, 5, 6});
    BOOST_ASSERT(ret == d.end());
    BOOST_ASSERT(d.back_free_capacity() > 0);
  }

  {
    Devector e = getRange<Devector, T>(8);
    auto ret = e.erase(e.end() - 3, e.end() - 1);
    assert_equals(e, {1, 2, 3, 4, 5, 8});
    BOOST_ASSERT(ret == e.end() - 1);
    BOOST_ASSERT(e.back_free_capacity() > 0);
  }

  {
    Devector f = getRange<Devector, T>(8);
    auto ret = f.erase(f.begin(), f.end());
    assert_equals(f, {});
    BOOST_ASSERT(ret == f.end());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_swap()
{
  using std::swap; // test if ADL works

  // empty-empty
  {
    Devector a;
    Devector b;

    swap(a, b);

    BOOST_ASSERT(a.empty());
    BOOST_ASSERT(b.empty());
  }

  // empty-not empty
  {
    Devector a;
    Devector b = getRange<Devector, T>(4);

    swap(a, b);

    BOOST_ASSERT(b.empty());
    assert_equals(a, {1, 2, 3, 4});

    swap(a, b);

    BOOST_ASSERT(a.empty());
    assert_equals(b, {1, 2, 3, 4});
  }

  // small-small / big-big
  {
    Devector a = getRange<Devector, T>(1, 5, 5, 7);
    Devector b = getRange<Devector, T>(13, 15, 15, 19);

    swap(a, b);

    assert_equals(a, {13, 14, 15, 16, 17, 18});
    assert_equals(b, {1, 2, 3, 4, 5, 6});

    swap(a, b);

    assert_equals(b, {13, 14, 15, 16, 17, 18});
    assert_equals(a, {1, 2, 3, 4, 5, 6});
  }

  // big-small + small-big
  {
    Devector a = getRange<Devector, T>(10);
    Devector b = getRange<Devector, T>(9, 11, 11, 17);

    swap(a, b);

    assert_equals(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    assert_equals(a, {9, 10, 11, 12, 13, 14, 15, 16});

    swap(a, b);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    assert_equals(b, {9, 10, 11, 12, 13, 14, 15, 16});
  }

  // self swap
  {
    Devector a = getRange<Devector, T>(10);

    swap(a, a);

    assert_equals(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  // no overlap
  {
    Devector a = getRange<Devector, T>(1, 9, 0, 0);
    Devector b = getRange<Devector, T>(0, 0, 11, 17);

    a.pop_back();
    a.pop_back();

    b.pop_front();
    b.pop_front();

    swap(a, b);

    assert_equals(a, {13, 14, 15, 16});
    assert_equals(b, {1, 2, 3, 4, 5, 6});
  }

  // exceptions

  bool can_throw =
     ! std::is_nothrow_move_constructible<T>::value
  && ! std::is_nothrow_copy_constructible<T>::value;

  // small-small with exception

  if (small_buffer_size<Devector>::back_size && can_throw)
  {
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(1,6,7,8);

    test_elem_throw::on_copy_after(4);
    test_elem_throw::on_move_after(4);

    try
    {
      swap(a, b);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}

    test_elem_throw::do_not_throw();
  }

  // failure in small-small swap has unspecified results but does not leak
  BOOST_ASSERT(test_elem_base::no_living_elem());

  // small-big with exception

  if (small_buffer_size<Devector>::back_size && can_throw)
  {
    Devector small = getRange<Devector, T>(8);
    Devector big = getRange<Devector, T>(32);

    std::vector<T> big_ex = getRange<std::vector<T>, T>(32);

    test_elem_throw::on_copy_after(4);
    test_elem_throw::on_move_after(4);

    try
    {
      swap(small, big);
      BOOST_ASSERT(false);
    } catch (const test_exception&) {}

    test_elem_throw::do_not_throw();

    // content of small might be moved
    assert_equals(big, big_ex);
  }

  // big-big does not copy or move
  {
    Devector a = getRange<Devector, T>(16);
    Devector b = getRange<Devector, T>(16);
    std::vector<T> c = getRange<std::vector<T>, T>(16);

    test_elem_throw::on_copy_after(1);
    test_elem_throw::on_move_after(1);

    swap(a, b);

    test_elem_throw::do_not_throw();

    assert_equals(a, c);
    assert_equals(b, c);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_clear()
{
  {
    Devector a;
    a.clear();
    BOOST_ASSERT(a.empty());
  }

  {
    Devector a = getRange<Devector, T>(8);
    a.clear();
    BOOST_ASSERT(a.empty());
    a.reset_alloc_stats();

    for (unsigned i = 0; i < small_buffer_size<Devector>::front_size; ++i)
    {
      a.emplace_front(i);
    }

    for (unsigned i = 0; i < small_buffer_size<Devector>::back_size; ++i)
    {
      a.emplace_back(i);
    }

    BOOST_ASSERT(a.capacity_alloc_count == 0);
    a.clear();
    BOOST_ASSERT(a.empty());
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_op_eq()
{
  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(a == b);
  }

  { // diff size
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(9);

    BOOST_ASSERT(!(a == b));
  }

  { // diff content
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(2,6,6,10);

    BOOST_ASSERT(!(a == b));
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_op_lt()
{
  { // little than
    Devector a = getRange<Devector, T>(7);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(a < b);
  }

  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(!(a < b));
  }

  { // greater than
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(7);

    BOOST_ASSERT(!(a < b));
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_op_ne()
{
  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(!(a != b));
  }

  { // diff size
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(9);

    BOOST_ASSERT(a != b);
  }

  { // diff content
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(2,6,6,10);

    BOOST_ASSERT(a != b);
  }
}


template <typename Devector, typename T = typename Devector::value_type>
void test_op_gt()
{
  { // little than
    Devector a = getRange<Devector, T>(7);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(!(a > b));
  }

  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(!(a > b));
  }

  { // greater than
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(7);

    BOOST_ASSERT(a > b);
  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_op_ge()
{
  { // little than
    Devector a = getRange<Devector, T>(7);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(!(a >= b));
  }

  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(a >= b);
  }

  { // greater than
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(7);

    BOOST_ASSERT(a >= b);
  }
}


template <typename Devector, typename T = typename Devector::value_type>
void test_op_le()
{
  { // little than
    Devector a = getRange<Devector, T>(7);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(a <= b);
  }

  { // equal
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(8);

    BOOST_ASSERT(a <= b);
  }

  { // greater than
    Devector a = getRange<Devector, T>(8);
    Devector b = getRange<Devector, T>(7);

    BOOST_ASSERT(!(a <= b));
  }
}

template <typename Devector>
void test_all_copyable(std::true_type /* value_type is copyable */)
{
  test_constructor_n_copy<Devector>();
  test_constructor_input_range<Devector>();
  test_constructor_forward_range<Devector>();
  test_constructor_pointer_range<Devector>();
  test_copy_constructor<Devector>();
  test_assignment<Devector>();
  test_il_assignment<Devector>();
  test_assign_input_range<Devector>();
  test_assign_forward_range<Devector>();
  test_assign_pointer_range<Devector>();
  test_assign_il<Devector>();
  test_assign_n<Devector>();
  test_resize_front_copy<Devector>();
  test_resize_back_copy<Devector>();
  test_push_front<Devector>();
  test_unsafe_push_front<Devector>();
  test_push_back<Devector>();
  test_unsafe_push_back<Devector>();
  test_insert<Devector>();
  test_insert_n<Devector>();
  test_insert_range<Devector>();
  test_insert_init_list<Devector>();
}

template <typename>
void test_all_copyable(std::false_type /* value_type is not copyable */)
{}

template <typename Devector>
void test_all_default_constructable(std::true_type)
{
  test_constructor_n<Devector>();
  test_resize_front<Devector>();
  test_resize_back<Devector>();
}

template <typename>
void test_all_default_constructable(std::false_type)
{}

template <typename Devector, typename T = typename Devector::value_type>
void test_all()
{
  test_constructor_default<Devector>();
  test_constructor_allocator<Devector>();
  test_constructor_reserve_only<Devector>();
  test_move_constructor<Devector>();
  test_destructor<Devector>();
  test_move_assignment<Devector>();
  test_begin_end<Devector>();
  test_empty<Devector>();
  test_size<Devector>();

  test_all_copyable<Devector>(std::is_copy_constructible<T>{});
  test_all_default_constructable<Devector>(std::is_default_constructible<T>{});

  test_capacity<Devector>();
  test_reserve_front<Devector>();
  test_reserve_back<Devector>();
  test_shrink_to_fit();
  test_index_operator<Devector>();
  test_at<Devector>();
  test_front<Devector>();
  test_back<Devector>();
  test_data();
  test_emplace_front<Devector>();
  test_push_front_rvalue<Devector>();
  test_unsafe_push_front_rvalue<Devector>();
  test_pop_front<Devector>();
  test_emplace_back<Devector>();
  test_push_back_rvalue<Devector>();
  test_unsafe_push_back_rvalue<Devector>();
  test_pop_back<Devector>();
  test_emplace<Devector>();
  test_insert_rvalue<Devector>();
  test_erase<Devector>();
  test_erase_range<Devector>();
  test_swap<Devector>();
  test_clear<Devector>();

  test_op_eq<Devector>();
  test_op_lt<Devector>();
  test_op_ne<Devector>();
  test_op_gt<Devector>();
  test_op_ge<Devector>();
  test_op_le<Devector>();
}

int main()
{
  static_assert(
    sizeof(devector<unsigned>) <=
       sizeof(devector<unsigned>::pointer)
     + sizeof(devector<unsigned>::size_type) * 3
     + sizeof(devector<unsigned>::size_type) * 2 // statistics
     + sizeof(devector<unsigned>::size_type) // padding
    ,"devector too large"
  );

  // TODO test custom allocator

  using devector_u = devector<unsigned>;
  using small_devector_u = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_u = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_u = devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 8>>;

  using devector_reg = devector<regular_elem>;
  using small_devector_reg = devector<regular_elem, std::allocator<regular_elem>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_reg = devector<regular_elem, std::allocator<regular_elem>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_reg = devector<regular_elem, std::allocator<regular_elem>, devector_small_buffer_policy<0, 8>>;

  using devector_nxmov = devector<noex_move>;
  using small_devector_nxmov = devector<noex_move, std::allocator<noex_move>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_nxmov = devector<noex_move, std::allocator<noex_move>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_nxmov = devector<noex_move, std::allocator<noex_move>, devector_small_buffer_policy<0, 8>>;

  using devector_nxcop = devector<noex_copy>;
  using small_devector_nxcop = devector<noex_copy, std::allocator<noex_copy>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_nxcop = devector<noex_copy, std::allocator<noex_copy>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_nxcop = devector<noex_copy, std::allocator<noex_copy>, devector_small_buffer_policy<0, 8>>;

  using devector_mov = devector<only_movable>;
  using small_devector_mov = devector<only_movable, std::allocator<only_movable>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_mov = devector<only_movable, std::allocator<only_movable>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_mov = devector<only_movable, std::allocator<only_movable>, devector_small_buffer_policy<0, 8>>;

  using devector_nodef = devector<no_default_ctor>;
  using small_devector_nodef = devector<no_default_ctor, std::allocator<no_default_ctor>, devector_small_buffer_policy<8, 8>>;
  using fsmall_devector_nodef = devector<no_default_ctor, std::allocator<no_default_ctor>, devector_small_buffer_policy<8, 0>>;
  using bsmall_devector_nodef = devector<no_default_ctor, std::allocator<no_default_ctor>, devector_small_buffer_policy<0, 8>>;

  test_all<devector_u>();
  test_all<small_devector_u>();
  test_all<fsmall_devector_u>();
  test_all<bsmall_devector_u>();

  test_all<devector_reg>();
  test_all<small_devector_reg>();
  test_all<fsmall_devector_reg>();
  test_all<bsmall_devector_reg>();

  test_all<devector_nxmov>();
  test_all<small_devector_nxmov>();
  test_all<fsmall_devector_nxmov>();
  test_all<bsmall_devector_nxmov>();

  test_all<devector_nxcop>();
  test_all<small_devector_nxcop>();
  test_all<fsmall_devector_nxcop>();
  test_all<bsmall_devector_nxcop>();

  test_all<devector_mov>();
  test_all<small_devector_mov>();
  test_all<fsmall_devector_mov>();
  test_all<bsmall_devector_mov>();

  test_all<devector_nodef>();
  test_all<small_devector_nodef>();
  test_all<fsmall_devector_nodef>();
  test_all<bsmall_devector_nodef>();

  BOOST_ASSERT(test_elem_base::no_living_elem());

  return 0;
}
