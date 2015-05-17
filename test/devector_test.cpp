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

int test_reserve()
{
  devector<unsigned> dv;
  dv.reserve_back(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    dv.push_back(i);
  }

  if (dv.capacity_alloc_count != 1) return 1;

  dv.reserve_front(100);
  for (unsigned i = 0; i < 100; ++i)
  {
    dv.push_front(i);
  }

  if (dv.capacity_alloc_count != 2) return 2;

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
    BOOST_ASSERT(boost::algorithm::equal(source.begin(), source.end(), f.begin(), f.end()));

    devector_u g{1, 2, 3};
    BOOST_ASSERT(boost::algorithm::equal(source.begin(), source.end(), g.begin(), g.end()));

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
    BOOST_ASSERT(boost::algorithm::equal(source.begin(), source.end(), f.begin(), f.end()));

    small_devector_u g{1, 2, 3};
    BOOST_ASSERT(boost::algorithm::equal(source.begin(), source.end(), g.begin(), g.end()));

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
      devector<throwing_elem> g(8, elem);
      BOOST_ASSERT(false);
    }
    catch (...) {}
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

  int err = 0;
  if ((err = test_push_pop()))  return 10 + err;
  if ((err = test_range_for())) return 20 + err;
  if ((err = test_reserve()))   return 30 + err;
  if ((err = test_push_front_back_alloc()))   return 40 + err;
  if ((err = test_small_buffer())) return 50 + err;

  return 0;
}

