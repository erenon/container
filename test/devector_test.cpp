#define BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
#include <boost/container/devector.hpp>
#undef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

using namespace boost::container;

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

  if (dv.capacity_alloc_count != 1) return 1;

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

  int err = 0;
  if ((err = test_push_pop()))  return 10 + err;
  if ((err = test_range_for())) return 20 + err;
  if ((err = test_reserve()))   return 30 + err;
  if ((err = test_push_front_back_alloc()))   return 40 + err;
  if ((err = test_small_buffer())) return 50 + err;

  return 0;
}

