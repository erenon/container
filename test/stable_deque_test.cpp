////////////////////////////////////////////////////////////////////////////////
////
//// (C) Copyright Benedek Thaler 2015-2015. Distributed under the Boost
//// Software License, Version 1.0. (See accompanying file
//// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////
//// See http://www.boost.org/libs/container for documentation.
////
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <forward_list>

#define BOOST_CONTAINER_TEST
#include <boost/container/stable_deque.hpp>
#undef BOOST_CONTAINER_TEST

using namespace boost::container;

#define BOOST_TEST_MODULE stable_deque
#include <boost/test/included/unit_test.hpp>

#include <boost/mpl/list.hpp>
#include <boost/mpl/filter_view.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/placeholders.hpp>

#include <boost/algorithm/cxx14/equal.hpp>

#include "test_elem.hpp"
#include "input_iterator.hpp"

template <typename T>
using make_stable_deque = stable_deque<T, std::allocator<T>, stable_deque_policy<8>>;

typedef boost::mpl::list<
  make_stable_deque<unsigned>,
  make_stable_deque<regular_elem>,
  make_stable_deque<noex_move>,
  make_stable_deque<noex_copy>,
  make_stable_deque<only_movable>,
  make_stable_deque<no_default_ctor>
> all_deques;

// TODO mpl::filter_view does not compile if predicate has template template argument. why?
//template <template<typename> class Predicate, typename Container>
//struct if_value_type
//  : public Predicate<typename Container::value_type>
//{};
//
//typedef boost::mpl::filter_view<all_deques, if_value_type<std::is_default_constructible, boost::mpl::_1>>::type
//  t_is_default_constructible;

template <typename Container>
struct is_value_type_default_constructible
  : public std::is_default_constructible<typename Container::value_type>
{};

typedef boost::mpl::filter_view<all_deques, is_value_type_default_constructible<boost::mpl::_1>>::type
  t_is_default_constructible;

template <typename Container>
struct is_value_type_copy_constructible
  : public std::is_copy_constructible<typename Container::value_type>
{};

typedef boost::mpl::filter_view<all_deques, is_value_type_copy_constructible<boost::mpl::_1>>::type
  t_is_copy_constructible;

template <typename Container>
struct is_value_type_trivial
  : public std::is_trivial<typename Container::value_type>
{};

typedef boost::mpl::filter_view<all_deques, is_value_type_trivial<boost::mpl::_1>>::type
  t_is_trivial;

template <typename Container>
Container get_range(int fbeg, int fend, int bbeg, int bend)
{
  Container c;

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

template <typename Container>
Container get_range(int count)
{
  Container c;

  for (int i = 1; i <= count; ++i)
  {
    c.emplace_back(i);
  }

  return c;
}

template <typename Container>
Container get_range()
{
  return get_range<Container>(1, 13, 13, 25);
}

template <typename T, typename A, typename P, typename C2>
bool operator==(const stable_deque<T, A, P>& a, const C2& b)
{
  return std::lexicographical_compare(
    a.begin(), a.end(),
    b.begin(), b.end()
  );
}

template <typename Range>
void print_range(std::ostream& out, const Range& range)
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

template <typename T, typename A, typename P>
std::ostream& operator<<(std::ostream& out, const stable_deque<T, A, P>& deque)
{
  print_range(out, deque);
  return out;
}

template <typename C1, typename C2>
void test_equal_range(const C1& a, const C2&b)
{
  bool equals = boost::algorithm::equal(
    a.begin(), a.end(),
    b.begin(), b.end()
  );

  BOOST_TEST(equals);

  if (!equals)
  {
    print_range(std::cerr, a);
    std::cerr << "\n";
    print_range(std::cerr, b);
    std::cerr << "\n";
  }
}

// support initializer_list
template <typename C>
void test_equal_range(const C& a, std::initializer_list<unsigned> il)
{
  typedef typename C::value_type T;
  std::vector<T> b;

  for (auto&& elem : il)
  {
    b.emplace_back(elem);
  }

  test_equal_range(a, b);
}

// END HELPERS

// TODO iterator operations

BOOST_AUTO_TEST_CASE_TEMPLATE(segment_iterator, Deque, t_is_trivial)
{
  typedef typename Deque::value_type T;

  devector<T> expected = get_range<devector<T>>();
  Deque a = get_range<Deque>();

  T* p_expected = expected.data();

  auto a_first = a.segment_begin();
  auto a_last = a.segment_end();

  while (a_first != a_last)
  {
    auto size = a_first.data_size();
    bool ok = std::memcmp(*a_first, p_expected, size) == 0;
    BOOST_TEST(ok);

    ++a_first;
    p_expected += size;
  }

  BOOST_TEST(p_expected == expected.data() + expected.size());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(consturctor_default, Deque, all_deques)
{
  {
    Deque a;

    BOOST_TEST(a.size() == 0u);
    BOOST_TEST(a.empty());
  }

  {
    typename Deque::allocator_type allocator;
    Deque b(allocator);

    BOOST_TEST(b.size() == 0);
    BOOST_TEST(b.empty());
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(constructor_n_value, Deque, t_is_default_constructible)
{
  typedef typename Deque::value_type T;

  {
    Deque a(0);
    BOOST_TEST(a.empty());
  }

  {
    Deque b(18);
    BOOST_TEST(b.size() == 18);

    const T tmp{};

    for (auto&& elem : b)
    {
      BOOST_TEST(elem == tmp);
    }
  }

  {
    Deque b(8);
    BOOST_TEST(b.size() == 8);

    const T tmp{};

    for (auto&& elem : b)
    {
      BOOST_TEST(elem == tmp);
    }
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    test_elem_throw::on_ctor_after(10);

    try
    {
      Deque a(12);
      BOOST_TEST(false);
    } catch (const test_exception&) {}

    BOOST_TEST(test_elem_base::no_living_elem());
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(constructor_n_copy, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;

  {
    const T x(9);
    Deque a(0, x);
    BOOST_TEST(a.empty());
  }

  {
    const T x(9);
    Deque b(18, x);
    BOOST_TEST(b.size() == 18);

    for (auto&& elem : b)
    {
      BOOST_TEST(elem == x);
    }
  }

  {
    const T x(9);
    Deque b(8, x);
    BOOST_TEST(b.size() == 8);

    for (auto&& elem : b)
    {
      BOOST_TEST(elem == x);
    }
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    test_elem_throw::on_copy_after(10);

    try
    {
      const T x(9);
      Deque a(12, x);
      BOOST_TEST(false);
    } catch (const test_exception&) {}

    BOOST_TEST(test_elem_base::no_living_elem());
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(constructor_input_range, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;

  {
    const devector<T> expected = get_range<devector<T>>(18);
    devector<T> input = expected;

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    Deque a(input_begin, input_end);
    BOOST_TEST(a == expected, boost::test_tools::per_element());
  }

  { // empty range
    devector<T> input;
    auto input_begin = make_input_iterator(input, input.begin());

    Deque b(input_begin, input_begin);

    BOOST_TEST(b.empty());
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    devector<T> input = get_range<devector<T>>(18);

    auto input_begin = make_input_iterator(input, input.begin());
    auto input_end   = make_input_iterator(input, input.end());

    test_elem_throw::on_copy_after(17);

    try
    {
      Deque c(input_begin, input_end);
      BOOST_TEST(false);
    } catch (const test_exception&) {}
  }

  BOOST_TEST(test_elem_base::no_living_elem());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(constructor_forward_range, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;
  const std::forward_list<T> x{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  {
    Deque a(x.begin(), x.end());
    test_equal_range(a, x);
  }

  {
    Deque b(x.begin(), x.begin());
    BOOST_TEST(b.empty());
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    test_elem_throw::on_copy_after(10);

    try
    {
      Deque c(x.begin(), x.end());
      BOOST_TEST(false);
    } catch (const test_exception&) {}
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(copy_constructor, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;

  {
    const Deque a;
    Deque b(a);

    BOOST_TEST(b.empty());
  }

  {
    const Deque a = get_range<Deque>();
    Deque b(a);

    test_equal_range(a, b);
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Deque a = get_range<Deque>();

    test_elem_throw::on_copy_after(12);

    try
    {
      Deque b(a);
      BOOST_TEST(false);
    } catch (const test_exception&) {}
  }

  BOOST_TEST(test_elem_base::no_living_elem());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(move_constructor, Deque, all_deques)
{
  { // empty
    Deque a;
    Deque b(std::move(a));

    BOOST_TEST(a.empty());
    BOOST_TEST(b.empty());
  }

  {
    Deque a = get_range<Deque>(1, 5, 5, 9);
    Deque b(std::move(a));

    test_equal_range(b, get_range<Deque>(1, 5, 5, 9));

    // a is unspecified but valid state
    a.clear();
    BOOST_TEST(a.empty());
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(constructor_il, Deque, t_is_copy_constructible)
{
  {
    Deque a({});
    BOOST_TEST(a.empty());
  }

  {
    Deque b{1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(b, get_range<Deque>(8));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(destructor, Deque, all_deques)
{
  { Deque a; }
  {
    Deque b = get_range<Deque>();
  }
}

// TODO op_copy_assign
// TODO op_move_assign
// TODO op_assign_il
// TODO assign_input_range
// TODO assign_forward_range
// TODO assign_n_copy
// TODO assign_il

BOOST_AUTO_TEST_CASE_TEMPLATE(get_allocator, Deque, all_deques)
{
  Deque a;
  (void)a.get_allocator();
}

template <typename Deque, typename MutableDeque>
void test_begin_end_impl()
{
  {
    Deque a;
    BOOST_TEST(a.begin() == a.end());
  }

  {
    Deque b = get_range<MutableDeque>(1, 13, 13, 25);
    auto expected = get_range<std::vector<typename Deque::value_type>>(24);

    BOOST_TEST(boost::algorithm::equal(
      b.begin(), b.end(),
      expected.begin(), expected.end()
    ));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(begin_end, Deque, all_deques)
{
  test_begin_end_impl<      Deque, Deque>();
  test_begin_end_impl<const Deque, Deque>();
}

template <typename Deque, typename MutableDeque>
void test_rbegin_rend_impl()
{
  {
    Deque a;
    BOOST_TEST(a.rbegin() == a.rend());
  }

  {
    Deque b = get_range<MutableDeque>(1, 13, 13, 25);
    auto expected = get_range<std::vector<typename Deque::value_type>>(24);

    BOOST_TEST(boost::algorithm::equal(
      b.rbegin(), b.rend(),
      expected.rbegin(), expected.rend()
    ));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(rbegin_rend, Deque, all_deques)
{
  test_rbegin_rend_impl<      Deque, Deque>();
  test_rbegin_rend_impl<const Deque, Deque>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(cbegin_cend, Deque, all_deques)
{
  {
    Deque a;
    BOOST_TEST(a.cbegin() == a.cend());
  }

  {
    Deque b = get_range<Deque>(1, 13, 13, 25);
    auto expected = get_range<std::vector<typename Deque::value_type>>(24);

    BOOST_TEST(boost::algorithm::equal(
      b.cbegin(), b.cend(),
      expected.cbegin(), expected.cend()
    ));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(crbegin_crend, Deque, all_deques)
{
  {
    Deque a;
    BOOST_TEST(a.crbegin() == a.crend());
  }

  {
    Deque b = get_range<Deque>(1, 13, 13, 25);
    auto expected = get_range<std::vector<typename Deque::value_type>>(24);

    BOOST_TEST(boost::algorithm::equal(
      b.crbegin(), b.crend(),
      expected.crbegin(), expected.crend()
    ));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(empty, Deque, all_deques)
{
  Deque a;
  BOOST_TEST(a.empty());

  a.emplace_front(1);

  BOOST_TEST(! a.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(size, Deque, all_deques)
{
  Deque a;
  BOOST_TEST(a.size() == 0u);

  a.emplace_front(1);
  a.emplace_front(2);
  a.emplace_front(3);

  BOOST_TEST(a.size() == 3u);

  a.pop_front();
  a.pop_front();

  BOOST_TEST(a.size() == 1u);

  a.emplace_back(2);
  a.emplace_back(3);
  a.emplace_back(4);
  a.emplace_back(5);
  a.emplace_back(6);

  BOOST_TEST(a.size() == 6u);

  a.emplace_back(7);
  a.emplace_back(8);
  a.emplace_back(9);
  a.emplace_back(10);
  a.emplace_back(11);

  BOOST_TEST(a.size() == 11u);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(max_size, Deque, all_deques)
{
  Deque a;
  (void)a.max_size();
}

// TODO resize_value
// TODO resize_copy

BOOST_AUTO_TEST_CASE_TEMPLATE(shrink_to_fit, Deque, all_deques)
{
  Deque a;
  a.shrink_to_fit();

  a.emplace_front(1);
  a.pop_front();
  a.shrink_to_fit();

  a.emplace_front(1);
  a.shrink_to_fit();
}

template <typename Deque, typename MutableDeque>
void test_op_at_impl()
{
  typedef typename Deque::value_type T;

  MutableDeque a = get_range<MutableDeque>(26);

  a.pop_front();
  a.pop_front();

  Deque b(std::move(a));

  BOOST_TEST(b[0] == T(3));
  BOOST_TEST(b[8] == T(11));
  BOOST_TEST(b[14] == T(17));
  BOOST_TEST(b[23] == T(26));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(op_at, Deque, all_deques)
{
  test_op_at_impl<      Deque, Deque>();
  test_op_at_impl<const Deque, Deque>();
}

template <typename Deque, typename MutableDeque>
void test_at_impl()
{
  typedef typename Deque::value_type T;

  MutableDeque a = get_range<MutableDeque>(26);

  a.pop_front();
  a.pop_front();

  Deque b(std::move(a));

  BOOST_TEST(b.at(0) == T(3));
  BOOST_TEST(b.at(8) == T(11));
  BOOST_TEST(b.at(14) == T(17));
  BOOST_TEST(b.at(23) == T(26));

  BOOST_CHECK_THROW(b.at(24), std::out_of_range);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(at, Deque, all_deques)
{
  test_at_impl<      Deque, Deque>();
  test_at_impl<const Deque, Deque>();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(front, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  { // non-const front
    Deque a = get_range<Deque>(3);
    BOOST_TEST(a.front() == T(1));
    a.front() = T(100);
    BOOST_TEST(a.front() == T(100));
  }

  { // const front
    const Deque a = get_range<Deque>(3);
    BOOST_TEST(a.front() == T(1));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(back, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  { // non-const front
    Deque a = get_range<Deque>(3);
    BOOST_TEST(a.back() == T(3));
    a.back() = T(100);
    BOOST_TEST(a.back() == T(100));
  }

  { // const front
    const Deque a = get_range<Deque>(3);
    BOOST_TEST(a.back() == T(3));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(emplace_front, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    a.emplace_front(3);
    a.emplace_front(2);
    a.emplace_front(1);

    test_equal_range(a, {1, 2, 3});
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Deque b = get_range<Deque>(8);

    try
    {
      test_elem_throw::on_ctor_after(1);
      b.emplace_front(404);
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(push_front_copy, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    for (std::size_t i = 1; i <= 12; ++i)
    {
      const T elem(i);
      a.push_front(elem);
    }

    test_equal_range(a, {12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Deque b = get_range<Deque>(10);

    try
    {
      const T elem(404);
      test_elem_throw::on_copy_after(1);
      b.push_front(elem);
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(push_front_rvalue, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    for (std::size_t i = 1; i <= 12; ++i)
    {
      T elem(i);
      a.push_front(std::move(elem));
    }

    test_equal_range(a, {12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  }

  if (! std::is_nothrow_move_constructible<T>::value)
  {
    Deque b = get_range<Deque>(8);

    try
    {
      test_elem_throw::on_move_after(1);
      b.push_front(T(404));
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(emplace_back, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    a.emplace_back(1);
    a.emplace_back(2);
    a.emplace_back(3);

    test_equal_range(a, {1, 2, 3});
  }

  if (! std::is_nothrow_constructible<T>::value)
  {
    Deque b = get_range<Deque>(8);

    try
    {
      test_elem_throw::on_ctor_after(1);
      b.emplace_back(404);
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(push_back_copy, Deque, t_is_copy_constructible)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    for (std::size_t i = 1; i <= 12; ++i)
    {
      const T elem(i);
      a.push_back(elem);
    }

    test_equal_range(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  }

  if (! std::is_nothrow_copy_constructible<T>::value)
  {
    Deque b = get_range<Deque>(10);

    try
    {
      const T elem(404);
      test_elem_throw::on_copy_after(1);
      b.push_back(elem);
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(push_back_rvalue, Deque, all_deques)
{
  typedef typename Deque::value_type T;

  {
    Deque a;

    for (std::size_t i = 1; i <= 12; ++i)
    {
      T elem(i);
      a.push_back(std::move(elem));
    }

    test_equal_range(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  }

  if (! std::is_nothrow_move_constructible<T>::value)
  {
    Deque b = get_range<Deque>(8);

    try
    {
      test_elem_throw::on_move_after(1);
      b.push_back(T(404));
      BOOST_TEST(false);
    }
    catch (const test_exception&) {}

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8});
  }
}

// TODO emplace

// TODO insert_copy
// TODO insert_rvalue
// TODO insert_n_copy
// TODO insert_input_range
// TODO insert_forward_range
// TODO insert_il
// TODO stable_insert

BOOST_AUTO_TEST_CASE_TEMPLATE(pop_front, Deque, all_deques)
{
  {
    Deque a;
    a.emplace_front(1);
    a.pop_front();
    BOOST_TEST(a.empty());

    a.emplace_back(2);
    a.pop_front();
    BOOST_TEST(a.empty());

    a.emplace_front(3);
    a.pop_front();
    BOOST_TEST(a.empty());
  }

  {
    Deque b = get_range<Deque>(20);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_TEST(!b.empty());
      b.pop_front();
    }
    BOOST_TEST(b.empty());
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(pop_back, Deque, all_deques)
{
  {
    Deque a;
    a.emplace_front(1);
    a.pop_back();
    BOOST_TEST(a.empty());

    a.emplace_back(2);
    a.pop_back();
    BOOST_TEST(a.empty());

    a.emplace_front(3);
    a.pop_back();
    BOOST_TEST(a.empty());
  }

  {
    Deque b = get_range<Deque>(20);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_TEST(!b.empty());
      b.pop_back();
    }
    BOOST_TEST(b.empty());
  }
}

// TODO erase
// TODO erase_range

BOOST_AUTO_TEST_CASE_TEMPLATE(member_swap, Deque, all_deques)
{
  {
    Deque a;
    Deque b;

    a.swap(b);

    BOOST_TEST(a.empty());
    BOOST_TEST(b.empty());
  }

  {
    Deque a;
    Deque b = get_range<Deque>(4);

    a.swap(b);

    test_equal_range(a, {1, 2, 3, 4});
    BOOST_TEST(b.empty());
  }

  {
    Deque a = get_range<Deque>(5, 9, 9, 13);
    Deque b = get_range<Deque>(4);

    a.swap(b);

    test_equal_range(a, {1, 2, 3, 4});
    test_equal_range(b, {5, 6, 7, 8, 9, 10, 11, 12});
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(clear, Deque, all_deques)
{
  {
    Deque a;
    a.clear();
    BOOST_TEST(a.empty());
  }

  {
    Deque b = get_range<Deque>();
    b.clear();
    BOOST_TEST(b.empty());
  }
}

// TODO op_eq
// TODO op_lt
// TODO op_ne
// TODO op_gt
// TODO op_ge
// TODO op_le
// TODO free_swap
