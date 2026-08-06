#pragma once
#include <concepts>

namespace JSlang::Math
{
template <typename Type>
    requires std::integral<Type>
constexpr Type CountDigits(Type Value)
{
    if (Value == 0)
    {
        return 1;
    }

    Type digit_count = 0;
    if constexpr (std::is_signed_v<Type>)
    {
        if (Value < 0)
        {
            while (Value != 0)
            {
                Value /= 10;
                digit_count++;
            }
            return digit_count;
        }
    }

    while (Value > 0)
    {
        Value /= 10;
        digit_count++;
    }
    return digit_count;
}
} // namespace JSlang::Math
