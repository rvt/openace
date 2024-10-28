#include "coreutils.hpp"
#include "egm96_dem.hpp"

#if defined(__MACH__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif


const etl::vector<OpenAce::Modulename, 7> CoreUtils::parsePath(const etl::string_view path)
{
    using StringView = etl::string_view ;
    using Vector     = etl::vector<OpenAce::Modulename, 7>;
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

int8_t CoreUtils::egm96GeoidOffset(float lat, float lon)
{
  int32_t ilat = roundf((90.f - lat) / 2.f);
  int32_t ilon = roundf(CoreUtils::toBearing(lon) / 2.f);
  int32_t offset = ilat * 180 + ilon;

  if (offset < 0 || static_cast<uint32_t>(offset) >= egm96s_dem.size())
  {
    return 0;
  }

  return egm96s_dem[offset];
}
