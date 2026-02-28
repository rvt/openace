#include "../moreutils.hpp"

uint32_t parseIpv4String(const etl::string_view ipStr, uint32_t defaultValue)
{
    using Token = etl::optional<etl::string_view>;
    uint32_t ip = 0xffffffffUL;
    uint8_t shift = 0;
    Token token;
    while ((token = etl::get_token(ipStr, ".", token, true)))
    {
        uint32_t value = atoi(token.value().cbegin());
        if (value > 255)
        {
            return defaultValue;
        }
        ip |= (value << shift);
        shift += 8;
    }

    return ip;
}
