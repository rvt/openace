#pragma once

#include <stdint.h>

/**
 * @brief Guards against recursive calls. It can potentially prevent stack races, use with care...
 * 
 * @tparam C 
 */
template <uint8_t C>
class RecursiveGuard
{
private:
    uint8_t &_count;
    const etl::string_view msg_ = "RecursiveGuard: Hit";
public:
    RecursiveGuard(uint8_t &ref) : _count(ref) { _count += 1; }
    RecursiveGuard(uint8_t &ref, etl::string_view msg) : _count(ref), msg_(msg) { _count += 1; }
    ~RecursiveGuard()
    {
        _count--;
    }

    RecursiveGuard(const RecursiveGuard &) = delete;
    RecursiveGuard &operator=(const RecursiveGuard &) = delete;
    RecursiveGuard(RecursiveGuard &&) = delete;
    RecursiveGuard &operator=(RecursiveGuard &&) = delete;

    operator bool() const
    {
        if (_count < C) {
            return true;
        } else {
            GATAS_INFO(msg_.data());
            return false;
        };
    }
};