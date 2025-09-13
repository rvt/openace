#include "../coreutils.hpp"
#include "../egm2008_dem.hpp"

#include "etl/string_utilities.h"

#if defined(__MACH__)
#include "pico/stdlib.h"
#else
#include <malloc.h>
#endif


const etl::vector<GATAS::Modulename, 7> CoreUtils::parsePath(const etl::string_view path)
{
    using StringView = etl::string_view ;
    using Vector     = etl::vector<GATAS::Modulename, 7>;
    using Token      = etl::optional<StringView>;

    Vector tokens;
    Token token;
    while ((token = etl::get_token(path, "/.?&", token, true)))
    {
        tokens.emplace_back(token.value());
    }
    return tokens;
}

uint32_t CoreUtils::getTotalHeap(void)
{
#if !defined(__arm__)
    return 0;
#else
    extern char __StackLimit, __bss_end__;
    return &__StackLimit  - &__bss_end__;
#endif

}

uint32_t CoreUtils::getFreeHeap(void)
{
// We hit this during unit testing, we return 0 because it would
// properly be useless
#if !defined(__arm__)
    return 0;
#else
    struct mallinfo m = mallinfo();
    return getTotalHeap() - m.uordblks;
#endif

}

int8_t CoreUtils::egmGeoidOffset(float lat, float lon)
{
    if (lat < egm2008_min_lat || lat > egm2008_max_lat ||
        lon < egm2008_min_lon || lon > egm2008_max_lon) {
//            printf("Latitude or longitude out of bounds: lat=%.2f, lon=%.2f\n", lat, lon);
        return 0;
    }

    int lat_idx = static_cast<int>(round((egm2008_max_lat - lat) / egm2008_resolution_deg));
    int lon_idx = static_cast<int>(round((lon - egm2008_min_lon) / egm2008_resolution_deg));

    if (lat_idx < 0 || lat_idx >= egm2008_lat_steps ||
        lon_idx < 0 || lon_idx >= egm2008_lon_steps) {
//        printf("Latitude or longitude index out of bounds: lat_idx=%d, lon_idx=%d\n", lat_idx, lon_idx);
        return 0;
    }

    int8_t val = egm2008s_dem[lon_idx][lat_idx];
    return (val == -128) ? 0 : val;
}