#include <boost/container/devector.hpp>

namespace boost{
namespace container {
namespace test{

}  //namespace test{
}  //namespace container {
}  //namespace boost{

using namespace boost::container;

int main()
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

  for (unsigned i = 0; i < 100; ++i)
  {
    dv.push_front(i);
  }

  unsigned exp = 99;
  for (auto&& act : dv)
  {
    if (act != exp) return 7;
    --exp;
  }

  return 0;
}

