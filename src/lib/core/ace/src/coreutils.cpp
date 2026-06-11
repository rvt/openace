#include "../coreutils.hpp"
#include "../egm2008_dem.hpp"

#include "etl/string_utilities.h"

#if defined(__MACH__)
#include "pico/stdlib.h"
#else
#include <malloc.h>
#endif

namespace
{
uint64_t offsetTimeToAbsolute = 0;
uint32_t timeUs32PpsOffset = 0;
spin_lock_t *claimedSpinLock = nullptr;
}

namespace CoreUtils
{
void init()
{
    claimedSpinLock = SpinlockGuard::claim();
}

spin_lock_t *sharedSpinLock()
{
    return claimedSpinLock;
}

uint32_t timeUs32()
{
    return time_us_32() - timeUs32PpsOffset;
}

uint64_t timeUs64()
{
    // time_us_64 and time_us_32 use the same hardware time, thus offset is also the same
    return time_us_64() - timeUs32PpsOffset;
}

void __time_critical_func(setPPS)(int32_t offsetUs)
{
    timeUs32PpsOffset = time_us_32() % 1'000'000 - offsetUs;
}

void setOffsetMsSinceEpoch(uint64_t msSinceEpoch)
{
    const uint64_t nowUs = time_us_64();
    offsetTimeToAbsolute = msSinceEpoch - nowUs / 1'000;
}

uint64_t msSinceEpoch()
{
    return (time_us_64() / 1'000) + offsetTimeToAbsolute;
}

const etl::vector<GATAS::Modulename, 7> parsePath(const etl::string_view path, const etl::string_view key)
{
    GATAS::ConfigString totalPath(path);
    if (key.size())
    {
        totalPath.append("/");
        totalPath.append(key);
    }
    using StringView = etl::string_view;
    using Vector = etl::vector<GATAS::Modulename, 7>;
    using Token = etl::optional<StringView>;

    Vector tokens;
    Token token;
    while ((token = etl::get_token(totalPath, "/.?&", token, true)))
    {
        if (tokens.full())
        {
            break;
        }
        tokens.emplace_back(token.value());
    }
    return tokens;
}

uint32_t getTotalHeap(void)
{
#if !defined(__arm__)
    return 0;
#else
    extern char __StackLimit, __bss_end__;
    return &__StackLimit - &__bss_end__;
#endif
}

uint32_t getFreeHeap(void)
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

int8_t egmGeoidOffset(float lat, float lon)
{
    // Convert directly to index space
    constexpr float invRes = 1.0f / egm2008_resolution_deg;

    const int lat_idx = static_cast<int>((egm2008_max_lat - lat) * invRes + 0.5f);
    const int lon_idx = static_cast<int>((lon - egm2008_min_lon) * invRes + 0.5f);

    // Single bounds check (fast path)
    if ((unsigned)lat_idx >= egm2008_lat_steps || (unsigned)lon_idx >= egm2008_lon_steps)
    {
        return 0;
    }

    return egm2008s_dem[lon_idx][lat_idx];
}
}
