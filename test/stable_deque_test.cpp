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
Container getRange(int fbeg, int fend, int bbeg, int bend)
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
Container getRange(int count)
{
  Container c;

  for (int i = 1; i <= count; ++i)
  {
    c.emplace_back(i);
  }

  return c;
}

template <typename Container>
Container getRange()
{
  return getRange<Container>(1, 13, 13, 25);
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

template <typename T, typename A, typename P>
std::ostream& operator<<(std::ostream& out, const stable_deque<T, A, P>& deque)
{
  printRange(out, deque);
  return out;
}

// END HELPERS

// TODO iterator operations

BOOST_AUTO_TEST_CASE_TEMPLATE(segment_iterator, Deque, t_is_trivial)
{
  typedef typename Deque::value_type T;

  devector<T> expected = getRange<devector<T>>();
  Deque a = getRange<Deque>();

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

// TODO constructor_n_value
// TODO constructor_n_copy
// TODO constructor_input_range
// TODO constructor_forward_range
// TODO copy_constructor
// TODO move_constructor
// TODO constructor_il

BOOST_AUTO_TEST_CASE_TEMPLATE(destructor, Deque, all_deques)
{
  { Deque a; }
  {
    Deque b = getRange<Deque>();
  }
}

// TODO op_copy_assign
// TODO op_move_assign
// TODO op_assign_il
// TODO assign_input_range
// TODO assign_forward_range
// TODO assign_n_copy
// TODO assign_in

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
    Deque b = getRange<MutableDeque>(1, 13, 13, 25);
    auto expected = getRange<std::vector<typename Deque::value_type>>(24);

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
    Deque b = getRange<MutableDeque>(1, 13, 13, 25);
    auto expected = getRange<std::vector<typename Deque::value_type>>(24);

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
    Deque b = getRange<Deque>(1, 13, 13, 25);
    auto expected = getRange<std::vector<typename Deque::value_type>>(24);

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
    Deque b = getRange<Deque>(1, 13, 13, 25);
    auto expected = getRange<std::vector<typename Deque::value_type>>(24);

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
}

BOOST_AUTO_TEST_CASE_TEMPLATE(max_size, Deque, all_deques)
{
  Deque a;
  (void)a.max_size();
}

// TODO resize_value
// TODO resize_copy
// TODO shrink_to_fit

// TODO op_at -- const, non const
// TODO at -- const, non const

// TODO front -- const, non const
// TODO back -- const, non const

// TODO emplace_front
// TODO push_front_copy
// TODO push_front_rvalue

// TODO emplace_back
// TODO push_back_copy
// TODO push_back_rvalue

// TODO emplace

// TODO insert_copy
// TODO insert_rvalue
// TODO insert_n_copy
// TODO insert_input_range
// TODO insert_forward_range
// TODO insert_il
// TODO stable_insert

// TODO pop_front
// TODO pop_back

// TODO erase
// TODO erase_range
// TODO swap
// TODO clear

// TODO op_eq
// TODO op_lt
// TODO op_ne
// TODO op_gt
// TODO op_ge
// TODO op_le
// TODO free_swap
