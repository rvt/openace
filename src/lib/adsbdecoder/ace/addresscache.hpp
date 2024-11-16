#pragma once

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/models.hpp"

#include "etl/flat_set.h"

template <size_t SIZE, uint32_t EVICT_TIME_US>
class AddressCache
{
    static constexpr uint32_t CLEAR_UP_SIZE = (SIZE * 90) / 100;

    struct AddressStatus
    {
        OpenAce::AircraftAddress icao;
        uint32_t lastSeen;
        AddressStatus(OpenAce::AircraftAddress icao_, uint32_t lastSeen_) : icao(icao_), lastSeen(lastSeen_)
        {
        }
        AddressStatus(const AddressStatus &other)
            : icao(other.icao), lastSeen(other.lastSeen)
        {
        }
        // copy assignment operator
        AddressStatus &operator=(const AddressStatus &other)
        {
            icao = other.icao;
            lastSeen = other.lastSeen;
            return *this;
        }
    };

    struct AddressComparator
    {
        constexpr bool operator()(const AddressStatus &lhs, const AddressStatus &rhs) const
        {
            return lhs.icao < rhs.icao;
        }
    };

    struct FindByIcao
    {
        FindByIcao(const OpenAce::AircraftAddress &icao) : icao(icao) {}
        bool operator()(const AddressStatus &i)
        {
            return i.icao == icao;
        }

    private:
        OpenAce::AircraftAddress icao;
    };

    etl::flat_set<AddressStatus, SIZE, AddressComparator> cache;

public:
    void clear() {
        cache.clear();
    }
    size_t size() const
    {
        return cache.size();
    }

    bool contains(uint32_t icao, uint32_t usTime)
    {
        auto it = etl::find_if(cache.begin(), cache.end(), FindByIcao(icao));

        if (it != cache.end())
        {
            it->lastSeen = usTime;
            return true;
        }

        return false;
    }

    bool insert(uint32_t address, uint32_t usTime)
    {
        if (cache.full())
        {
            evictOldEntries(usTime);
        }

        cache.insert(AddressStatus{address, usTime});
        return true;
    }

    void evictOldEntries(uint32_t usTime)
    {
        // Always ensure there is room for new cache entries be reducing evictTime untill there is room again
        auto evictTime = EVICT_TIME_US;
        while ((cache.size() > CLEAR_UP_SIZE) && evictTime > 2'000'000)
        {
            cache.erase(etl::remove_if(cache.begin(), cache.end(), [usTime, evictTime](const auto &it)
            {
                return CoreUtils::usElapsed(it.lastSeen, usTime) > evictTime;
            }),
            cache.end());
            evictTime -= 5'000'000;
        }
    }
};
