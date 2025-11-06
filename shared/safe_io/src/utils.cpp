#include "safe_io/utils.hpp"

#include <iostream>

namespace safe_io
{
    std::ostream& out() noexcept
    {
        return std::cout;
    }
} // namespace safe_io
