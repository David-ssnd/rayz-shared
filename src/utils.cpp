#include "utils.h"

std::string toBinaryString(uint32_t value, int bits)
{
    std::string binary = "";
    for (int i = bits - 1; i >= 0; i--)
    {
        binary += ((value >> i) & 1) ? "1" : "0";
        if (i > 0 && i % 4 == 0)
        {
            binary += " ";
        }
    }
    return binary;
}
