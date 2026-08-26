#pragma once
#include <array>
#include <cstdint>

namespace JSlang::BitwiseCharacterClassifier
{
enum CharacterFlags : uint8_t
{
    None       = 0,
    Alpha      = 1 << 0,
    Digit      = 1 << 1,
    Underscore = 1 << 2,
    Whitespace = 1 << 3,

    IdentifierStart = Alpha | Underscore,
    IdentifierBody  = Alpha | Digit | Underscore
};

alignas(64) static constexpr std::array<uint8_t, 256> ASCII_LUT = []() constexpr
{
    std::array<uint8_t, 256> LUT = {0};

    for (int i = 0; i < 256; i++)
    {
        if ((i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z'))
        {
            LUT[i] |= Alpha;
        }
        else if (i >= '0' && i <= '9')
        {
            LUT[i] |= Digit;
        }
        else if (i == '_')
        {
            LUT[i] |= Underscore;
        }
        else if (i == ' ' || i == '\t' || i == '\r' || i == '\n')
        {
            LUT[i] |= Whitespace;
        }
    }
    return LUT;
}();

bool IsIdentifierBody(char Character)
{
    return (ASCII_LUT[Character] & CharacterFlags::IdentifierBody) != 0;
}

bool IsIdentifierStart(char Character)
{
    return (ASCII_LUT[Character] & CharacterFlags::IdentifierStart) != 0;
}

bool IsWhitespace(char Character)
{
    return (ASCII_LUT[Character] & CharacterFlags::Whitespace) != 0;
}

} // namespace JSlang::BitwiseCharacterClassifier
