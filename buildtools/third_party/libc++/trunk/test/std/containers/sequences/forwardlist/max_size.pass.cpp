//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <forward_list>

// size_type max_size() const;

#include <cassert>
#include <forward_list>
#include <limits>
#include <type_traits>

#include "test_allocator.h"
#include "test_macros.h"

int main()
{
    {
      typedef limited_allocator<int, 10> A;
      typedef std::forward_list<int, A> C;
      C c;
      assert(c.max_size() <= 10);
      LIBCPP_ASSERT(c.max_size() == 10);
    }
    {
      typedef limited_allocator<int, (size_t)-1> A;
      typedef std::forward_list<int, A> C;
      const C::size_type max_dist =
          static_cast<C::size_type>(std::numeric_limits<C::difference_type>::max());
      C c;
      assert(c.max_size() <= max_dist);
      LIBCPP_ASSERT(c.max_size() == max_dist);
    }
    {
      typedef std::forward_list<char> C;
      const C::size_type max_dist =
          static_cast<C::size_type>(std::numeric_limits<C::difference_type>::max());
      C c;
      assert(c.max_size() <= max_dist);
      assert(c.max_size() <= alloc_max_size(c.get_allocator()));
    }
}
