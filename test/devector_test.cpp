#include <cstring> // memcmp
#include <iostream>

#define BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
#include <boost/container/devector.hpp>
#undef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

#include <boost/algorithm/cxx14/equal.hpp>

using namespace boost::container;

struct test_exception {};

struct throwing_elem
{
  static int throw_on_ctor_after /*= -1*/;
  static int throw_on_copy_after /*= -1*/;
  static int throw_on_move_after /*= -1*/;

  throwing_elem()
  {
    maybe_throw(throw_on_ctor_after);

    _dummy_member = new int(123);
  }

  ~throwing_elem()
  {
    if (_dummy_member)
    {
      delete _dummy_member;
    }
  }

  throwing_elem(const throwing_elem&)
  {
    maybe_throw(throw_on_copy_after);

    _dummy_member = new int(123);
  }

  throwing_elem(throwing_elem&& rhs)
  {
    maybe_throw(throw_on_move_after);

    _dummy_member = rhs._dummy_member;
    rhs._dummy_member = nullptr;
  }

private:
  void maybe_throw(int& counter)
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

  int* _dummy_member; // for leak detection
};

int throwing_elem::throw_on_ctor_after = -1;
int throwing_elem::throw_on_copy_after = -1;
int throwing_elem::throw_on_move_after = -1;

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

int test_push_pop()
{
  devector<unsigned> dv;

  if (! dv.empty()) return 1;

  dv.push_back(30);
  dv.push_back(40);
  dv.push_back(50);

  if (dv.size() != 3) return 2;

  dv.push_front(20);
  dv.push_front(10);
  dv.push_front(0);

  if (dv.size() != 6) return 4;

  for (unsigned i = 0; i < dv.size(); ++i)
  {
    if (dv[i] != i * 10) return 5;
  }

  dv.pop_front();
  dv.pop_back();
  dv.pop_front();
  dv.pop_back();
  dv.pop_front();
  dv.pop_back();

  if (! dv.empty()) return 6;

  return 0;
}

int test_range_for()
{
  devector<unsigned> dv;

  for (unsigned i = 0; i < 100; ++i)
  {
    dv.push_front(i);
  }

  unsigned exp = 99;
  for (auto&& act : dv)
  {
    if (act != exp) return 1;
    --exp;
  }

  return 0;
}

int test_push_front_back_alloc()
{
  devector<unsigned> dv;

  for (unsigned i = 0; i < 5; ++i)
  {
    dv.push_front(i);
    dv.push_back(i);
  }

  if (dv.capacity_alloc_count != 2) return 1;

  return 0;
}

int test_small_buffer()
{
  devector<unsigned, std::allocator<unsigned>, devector_store_n<128>> dv;

  for (unsigned i = 0; i < 128; ++i)
  {
    dv.push_back(i);
  }

  if (dv.capacity_alloc_count != 0) return 1;

  dv.push_back(0);

  if (dv.capacity_alloc_count != 1) return 2;

  return 0;
}

typedef devector<unsigned> devector_u;
typedef devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<8, 8>> small_devector_u;
typedef devector<throwing_elem, std::allocator<throwing_elem>, devector_small_buffer_policy<8, 8>> small_devector_thr;

void test_constructor()
{
  { // zero sized small buffer
    devector_u a;
    BOOST_ASSERT(a.size() == 0);
    BOOST_ASSERT(a.capacity() == 0);

    devector_u b(devector_u::allocator_type{});
    BOOST_ASSERT(b.size() == 0);
    BOOST_ASSERT(b.capacity() == 0);

    devector_u c(16, reserve_only_tag{});
    BOOST_ASSERT(c.size() == 0);
    BOOST_ASSERT(c.capacity() == 16);

    devector_u d(16);
    BOOST_ASSERT(d.size() == 16);
    BOOST_ASSERT(d.capacity() == 16);

    devector_u e(16, 123u);
    BOOST_ASSERT(e.size() == 16);
    BOOST_ASSERT(e.capacity() == 16);

    for (auto&& elem : e)
    {
      BOOST_ASSERT(elem == 123u);
    }

    std::vector<unsigned> source{1, 2, 3};

    devector_u f(source.begin(), source.end());
    assert_equals(f, source);

    devector_u g{1, 2, 3};
    assert_equals(g, source);

    try
    {
      throwing_elem::throw_on_ctor_after = 4;
      devector<throwing_elem> td(8);
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      throwing_elem::throw_on_copy_after = 4;
      throwing_elem elem;
      devector<throwing_elem> te(8, elem);
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      std::vector<throwing_elem> source(8);
      throwing_elem::throw_on_copy_after = 4;
      devector<throwing_elem> tf(source.begin(), source.end());
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      throwing_elem::throw_on_copy_after = 2;
      devector<throwing_elem> tg{throwing_elem{}, throwing_elem{}, throwing_elem{}};
      BOOST_ASSERT(false);
    }
    catch (...) {}
  }

  { // with small buffer
    small_devector_u a;
    BOOST_ASSERT(a.size() == 0);
    BOOST_ASSERT(a.capacity() == 16);

    small_devector_u b(small_devector_u::allocator_type{});
    BOOST_ASSERT(b.size() == 0);
    BOOST_ASSERT(b.capacity() == 16);

    small_devector_u c(6, reserve_only_tag{});
    BOOST_ASSERT(c.size() == 0);
    BOOST_ASSERT(c.capacity() == 16);

    small_devector_u d(6);
    BOOST_ASSERT(d.size() == 6);
    BOOST_ASSERT(d.capacity() == 16);

    small_devector_u e(6, 123u);
    BOOST_ASSERT(e.size() == 6);
    BOOST_ASSERT(e.capacity() == 16);

    for (auto&& elem : e)
    {
      BOOST_ASSERT(elem == 123u);
    }

    std::vector<unsigned> source{1, 2, 3};

    small_devector_u f(source.begin(), source.end());
    assert_equals(f, source);

    small_devector_u g{1, 2, 3};
    assert_equals(g, source);

    try
    {
      throwing_elem::throw_on_ctor_after = 4;
      small_devector_thr td(8);
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      throwing_elem::throw_on_copy_after = 4;
      throwing_elem elem;
      small_devector_thr te(8, elem);
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      std::vector<throwing_elem> source(8);
      throwing_elem::throw_on_copy_after = 4;
      small_devector_thr tf(source.begin(), source.end());
      BOOST_ASSERT(false);
    }
    catch (...) {}

    try
    {
      throwing_elem::throw_on_copy_after = 2;
      small_devector_thr tg{throwing_elem{}, throwing_elem{}, throwing_elem{}};
      BOOST_ASSERT(false);
    }
    catch (...) {}
  }
}

void test_copy_constructor()
{
  devector_u a{1, 2, 3, 4, 5};
  devector_u b(a);

  BOOST_ASSERT(a == b);

  devector_u c(a, std::allocator<unsigned>{});
  BOOST_ASSERT(a == c);

  small_devector_u d(a);
  BOOST_ASSERT(a == d);

  try
  {
    devector<throwing_elem> source(8);
    throwing_elem::throw_on_copy_after = 4;
    devector<throwing_elem> e(source);
    BOOST_ASSERT(false);
  }
  catch (...) {}
}

void test_destructor()
{
  devector_u a;
  small_devector_u b;
  devector<throwing_elem> c(3);
}

void test_begin_end()
{
  std::vector<unsigned> expected{1, 2, 3, 4, 5, 6, 7, 8};
  devector_u actual{1, 2, 3, 4, 5, 6, 7, 8};

  BOOST_ASSERT(boost::algorithm::equal(expected.begin(), expected.end(), actual.begin(), actual.end()));
  BOOST_ASSERT(boost::algorithm::equal(expected.rbegin(), expected.rend(), actual.rbegin(), actual.rend()));
  BOOST_ASSERT(boost::algorithm::equal(expected.cbegin(), expected.cend(), actual.cbegin(), actual.cend()));
  BOOST_ASSERT(boost::algorithm::equal(expected.crbegin(), expected.crend(), actual.crbegin(), actual.crend()));

  const devector_u cactual{1, 2, 3, 4, 5, 6, 7, 8};

  BOOST_ASSERT(boost::algorithm::equal(expected.begin(), expected.end(), cactual.begin(), cactual.end()));
  BOOST_ASSERT(boost::algorithm::equal(expected.rbegin(), expected.rend(), cactual.rbegin(), cactual.rend()));
}

void test_empty()
{
  devector_u a;
  BOOST_ASSERT(a.empty());

  small_devector_u b;
  BOOST_ASSERT(b.empty());

  devector_u c(16, reserve_only_tag{});
  BOOST_ASSERT(c.empty());

  devector_u d{1, 2, 3};
  BOOST_ASSERT(! d.empty());

  devector_u e(1);
  BOOST_ASSERT(! e.empty());
}

void test_size()
{
  devector_u a;
  BOOST_ASSERT(a.size() == 0);

  small_devector_u b;
  BOOST_ASSERT(b.size() == 0);

  devector_u c(16, reserve_only_tag{});
  BOOST_ASSERT(c.size() == 0);

  devector_u d{1, 2, 3};
  BOOST_ASSERT(d.size() == 3);

  devector_u e(3);
  BOOST_ASSERT(e.size() == 3);
}

void test_capacity()
{
  devector_u a;
  BOOST_ASSERT(a.capacity() == 0);

  small_devector_u b;
  BOOST_ASSERT(b.capacity() == 16);

  devector_u c(16, reserve_only_tag{});
  BOOST_ASSERT(c.capacity() == 16);
}

void test_resize_back()
{
  // size < required, alloc needed
  devector_u a{1, 2, 3, 4, 5};
  a.resize_back(8);
  assert_equals(a, {1, 2, 3, 4, 5, 0, 0, 0});

  // size < required, but capacity provided
  devector_u b{1, 2, 3, 4, 5};
  b.reserve(16);
  b.resize_back(8);
  assert_equals(b, {1, 2, 3, 4, 5, 0, 0, 0});

  // size < required, copy or move throws
  devector<throwing_elem> c(5);
  throwing_elem::throw_on_copy_after = 3;
  throwing_elem::throw_on_move_after = 3;

  try
  {
    c.resize(10);
    BOOST_ASSERT(false);
  }
  catch (...) {}

  // TODO test contents of c

  // size < required, constructor throws
  devector<throwing_elem> d(5);
  throwing_elem::throw_on_ctor_after = 3;

  try
  {
    d.resize(10);
    BOOST_ASSERT(false);
  }
  catch (...) {}

  // TODO test contents of d

  // size >= required
  devector_u e{1, 2, 3, 4, 5, 6};
  e.resize_back(4);
  assert_equals(e, {1, 2, 3, 4});
}

void test_resize_back_copy()
{
  unsigned x = 123;

  // size < required, alloc needed
  devector_u a{1, 2, 3, 4, 5};
  a.resize_back(8, x);
  assert_equals(a, {1, 2, 3, 4, 5, 123, 123, 123});

  // size < required, but capacity provided
  devector_u b{1, 2, 3, 4, 5};
  b.reserve(16);
  b.resize_back(8, x);
  assert_equals(b, {1, 2, 3, 4, 5, 123, 123, 123});

  // size < required, copy or move throws
  throwing_elem thr_x;

  devector<throwing_elem> c(5);
  throwing_elem::throw_on_copy_after = 3;
  throwing_elem::throw_on_move_after = 3;

  try
  {
    c.resize(10, thr_x);
    BOOST_ASSERT(false);
  }
  catch (...) {}

  // TODO test contents of c

  // size < required, constructor throws
  devector<throwing_elem> d(5);
  throwing_elem::throw_on_ctor_after = 3;

  try
  {
    d.resize(10, thr_x);
    BOOST_ASSERT(false);
  }
  catch (...) {}

  // TODO test contents of d

  // size >= required
  devector_u e{1, 2, 3, 4, 5, 6};
  e.resize_back(4, x);
  assert_equals(e, {1, 2, 3, 4});
}

void test_reserve_front()
{
  // test allocation statistics
  devector<unsigned> a;

  a.reserve_front(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    a.push_front(i);
  }

  BOOST_ASSERT(a.capacity_alloc_count == 1);

  // test SBO
  devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<8, 0>> b;
  b.reserve_front(4);
  b.reserve_front(6);
  b.reserve_front(4);

  BOOST_ASSERT(b.capacity_alloc_count == 0);

  // test SBO, front is empty
  devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 8>> c;
  c.reserve_front(4);
  c.reserve_front(6);
  c.reserve_front(4);

  BOOST_ASSERT(c.capacity_alloc_count == 2);
}

void test_reserve_back()
{
  // test allocation statistics
  devector<unsigned> a;

  a.reserve_back(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    a.push_back(i);
  }

  BOOST_ASSERT(a.capacity_alloc_count == 1);

  // test SBO, back is empty
  devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<0, 8>> b;
  b.reserve_back(4);
  b.reserve_back(6);
  b.reserve_back(4);

  BOOST_ASSERT(b.capacity_alloc_count == 0);

  // test SBO, back is empty
  devector<unsigned, std::allocator<unsigned>, devector_small_buffer_policy<8, 0>> c;
  c.reserve_back(4);
  c.reserve_back(6);
  c.reserve_back(4);

  BOOST_ASSERT(c.capacity_alloc_count == 2);
}

// TODO test_shrink_to_fit

void test_index_operator()
{
  { // non-const []
    devector_u a{1, 2, 3, 4, 5};

    BOOST_ASSERT(a[0] == 1);
    BOOST_ASSERT(a[4] == 5);
    BOOST_ASSERT(&a[3] == &a[0] + 3);

    a[0] = 100;
    BOOST_ASSERT(a[0] == 100);
  }

  { // const []
    const devector_u a{1, 2, 3, 4, 5};

    BOOST_ASSERT(a[0] == 1);
    BOOST_ASSERT(a[4] == 5);
    BOOST_ASSERT(&a[3] == &a[0] + 3);
  }
}

void test_at()
{
  { // non-const at
    devector_u a{1, 2, 3};

    BOOST_ASSERT(a.at(0) == 1);
    a.at(0) = 100;
    BOOST_ASSERT(a.at(0) == 100);

    try
    {
      a.at(3);
      BOOST_ASSERT(false);
    } catch (const std::out_of_range&) {}
  }

  { // const at
    const devector_u a{1, 2, 3};

    BOOST_ASSERT(a.at(0) == 1);

    try
    {
      a.at(3);
      BOOST_ASSERT(false);
    } catch (const std::out_of_range&) {}
  }
}

void test_front()
{
  { // non-const front
    devector_u a{1, 2, 3};
    BOOST_ASSERT(a.front() == 1);
    a.front() = 100;
    BOOST_ASSERT(a.front() == 100);
  }

  { // const front
    const devector_u a{1, 2, 3};
    BOOST_ASSERT(a.front() == 1);
  }
}

void test_back()
{
  { // non-const back
    devector_u a{1, 2, 3};
    BOOST_ASSERT(a.back() == 3);
    a.back() = 100;
    BOOST_ASSERT(a.back() == 100);
  }

  { // const back
    const devector_u a{1, 2, 3};
    BOOST_ASSERT(a.back() == 3);
  }
}

void test_data()
{
  unsigned c_array[] = {1, 2, 3, 4};

  { // non-const data
    devector_u a(c_array, c_array + 4);
    BOOST_ASSERT(a.data() == &a.front());

    BOOST_ASSERT(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);

    *(a.data()) = 100;
    BOOST_ASSERT(a.front() == 100);
  }

  { // const data
    const devector_u a(c_array, c_array + 4);
    BOOST_ASSERT(a.data() == &a.front());

    BOOST_ASSERT(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);
  }
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

  test_constructor();
  test_copy_constructor();
  test_destructor();
  test_begin_end();
  test_empty();
  test_size();
  test_capacity();
  test_resize_back();
  test_resize_back_copy();
  test_reserve_back();
  test_reserve_front();
  test_index_operator();
  test_at();
  test_front();
  test_back();
  test_data();

  int err = 0;
  if ((err = test_push_pop()))  return 10 + err;
  if ((err = test_range_for())) return 20 + err;
  if ((err = test_push_front_back_alloc()))   return 40 + err;
  if ((err = test_small_buffer())) return 50 + err;

  return 0;
}

