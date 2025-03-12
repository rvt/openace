#pragma once

#include <stdint.h>
#include "etl/vector.h"

#include "address.hpp"

namespace FANET
{
    static constexpr uint32_t NEIGHBOR_MAX_TIMEOUT_MS = 4 * 60 * 1000 + 10'000; // 4min + 10sek

    template <size_t FANET_MAX_NEIGHBORS>
    class NeighbourTable
    {
    private:
        struct Neighbour
        {
            Address address;
            uint32_t lastSeen;
        };
        etl::vector<Neighbour, FANET_MAX_NEIGHBORS> neighborTable;

    public:
        void clear()
        {
            return neighborTable.clear();
        }
        size_t size() const
        {
            return neighborTable.size();
        }

        void addOrUpdate(Address address, uint32_t lastSeen)
        {
            // THis is to simply the code, and 1ms is not a concern
            // So that lastSeen will always return a value
            if (neighborTable.full())
            {
                removeOldest();
            }

            auto it = std::find_if(neighborTable.begin(), neighborTable.end(), [&address](const Neighbour &n)
                                   { return n.address == address; });
            if (it != neighborTable.end())
            {
                it->lastSeen = lastSeen;
            }
            else
            {
                neighborTable.push_back(Neighbour{address, lastSeen});
            }
        }

        void remove(const Address &address)
        {
            neighborTable.erase(std::remove_if(neighborTable.begin(), neighborTable.end(), [&address](const Neighbour &neighbour)
                                               { return neighbour.address == address; }),
                                neighborTable.end());
        }

        uint32_t lastSeen(const Address &address) const
        {
            auto it = std::find_if(neighborTable.begin(), neighborTable.end(), [&address](const Neighbour &neighbour)
                                   { return neighbour.address == address; });
            if (it != neighborTable.end())
            {
                return it->lastSeen;
            }
            return 0;
        }


        void removeOldest()
        {
            auto it = std::min_element(neighborTable.begin(), neighborTable.end(), [](const Neighbour &a, const Neighbour &b)
                                       { return a.lastSeen < b.lastSeen; });
            if (it != neighborTable.end())
            {
                neighborTable.erase(it);
            }
        }


        void removeOutdated(uint32_t timeMs)
        {
            neighborTable.erase(std::remove_if(neighborTable.begin(), neighborTable.end(), [&timeMs](const Neighbour &neighbour)
                                               { 
                                                uint32_t diff = timeMs - neighbour.lastSeen;
                                                return diff >  NEIGHBOR_MAX_TIMEOUT_MS; }),
                                neighborTable.end());
        }
    };

}