#pragma once

#include <stdint.h>

template <uint8_t C>
class RecursiveGuard
{
private:
    uint8_t &_count;
    const etl::string_view msg_ = "RecursiveGuard: Hit";
public:
    RecursiveGuard(uint8_t &ref) : _count(ref) { _count++; }
    RecursiveGuard(uint8_t &ref, etl::string_view msg) : _count(ref), msg_(msg) { _count++; }
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
            puts(msg_.data());
            return false;
        };
    }
};