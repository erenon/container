#include <cstring> // memcmp
#include <iostream>

#define BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
#include <boost/container/devector.hpp>
#undef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

#include <boost/algorithm/cxx14/equal.hpp>
#include <boost/move/core.hpp> // BOOST_MOVABLE_BUT_NOT_COPYABLE

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

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_if_default_constructible(std::true_type /* T has default constructor */)
{
  Devector d(16);
  BOOST_ASSERT(d.size() == 16);
  BOOST_ASSERT(d.capacity() == 16);

  if (! std::is_nothrow_constructible<T>::value)
  {
    try
    {
      test_elem_throw::on_ctor_after(4);
      Devector td(8);
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}
  }
}

template <typename, typename>
void test_constructor_if_default_constructible(std::false_type /* T has no default constructor */)
{}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor_if_copyable(std::true_type /* T has copy constructor */)
{
  {
    T t(123);
    Devector e(16, t);

    BOOST_ASSERT(e.size() == 16);
    BOOST_ASSERT(e.capacity() == 16);

    std::vector<T> expected(16, t);
    assert_equals(e, expected);
  }

  {
    std::vector<T> expected{T(1), T(2), T(3)};

    // TODO use input iterator instead
    Devector f(expected.begin(), expected.end());
    assert_equals(f, expected);
  }

  {
    std::vector<T> expected{T(1), T(2), T(3)};
    Devector g{T(1), T(2), T(3)};
    assert_equals(g, expected);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {

    try
    {
      test_elem_throw::on_copy_after(4);
      Devector te(8, T(123));
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

    try
    {
      test_elem_throw::on_copy_after(2);
      Devector tg{T(1), T(2), T(3)};
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

    try
    {
      std::vector<T> source{T(1), T(2), T(3), T(4)};
      test_elem_throw::on_copy_after(2);
      Devector tf(source.begin(), source.end());
      BOOST_ASSERT(false);
    }
    catch (test_exception&) {}

  }
}

template <typename, typename>
void test_constructor_if_copyable(std::false_type /* T has no copy constructor */)
{}

template <typename Devector, typename T = typename Devector::value_type>
void test_constructor()
{
  Devector a;
  BOOST_ASSERT(a.size() == 0);
  BOOST_ASSERT(a.capacity_alloc_count == 0);

  Devector b(typename Devector::allocator_type{});
  BOOST_ASSERT(b.size() == 0);
  BOOST_ASSERT(b.capacity_alloc_count == 0);

  Devector c(16, reserve_only_tag{});
  BOOST_ASSERT(c.size() == 0);
  BOOST_ASSERT(c.capacity() >= 16);

  test_constructor_if_default_constructible<Devector, T>(std::is_default_constructible<T>{});
  test_constructor_if_copyable<Devector, T>(std::is_copy_constructible<T>{});
}

template <typename Devector, typename T = typename Devector::value_type>
void test_copy_constructor()
{
  {
    Devector a{T(1), T(2), T(3), T(4), T(5)};
    Devector b(a);

    BOOST_ASSERT(a == b);

    Devector c(a, std::allocator<T>{});
    BOOST_ASSERT(a == c);

    devector<T, std::allocator<T>, devector_small_buffer_policy<8,8>> d(a);
    BOOST_ASSERT(a == d);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {

    try
    {
      Devector source{T(1), T(2), T(3), T(4), T(5)};
      test_elem_throw::on_copy_after(4);
      Devector e(source);
      BOOST_ASSERT(false);
    }
    catch (const test_exception&) {}

  }
}

template <typename Devector, typename T = typename Devector::value_type>
void test_destructor()
{
  Devector a;

  Devector b = getRange<Devector, T>(3);
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

template <typename>
struct small_buffer_size;

template <typename U, typename A, typename SBP, typename GP>
struct small_buffer_size<devector<U, A, SBP, GP>>
{
  static const unsigned value = SBP::size;
};

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
  struct always_shrink : public devector_default_growth_policy
  {
    static bool should_shrink(unsigned, unsigned, unsigned)
    {
      return true;
    }
  };

  struct never_shrink : public devector_default_growth_policy
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

// TODO test erase
// TODO test erase range
// TODO test swap
// TODO test clear
// TODO test comparison operators

template <typename Devector>
void test_all_copyable(std::true_type /* value_type is copyable */)
{
  test_copy_constructor<Devector>();
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
  test_resize_front<Devector>();
  test_resize_back<Devector>();
}

template <typename>
void test_all_default_constructable(std::false_type)
{}

template <typename Devector, typename T = typename Devector::value_type>
void test_all()
{
  test_constructor<Devector>();
  test_destructor<Devector>();
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
