#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include "etl/span.h"

/**
 * Small helper class to do something every XX
 * Every assumes that isItTime(..) will provide an increasing counter
 * By specifying an 'at' can be used to avoid hotspots where all functions will do something at teh same time.
 */
template<typename T, T at, T every>
class Every
{
    T next;
public:
    Every() : next(0) {}
    Every(T n) : next(n) {}

    bool isItTime(T t)
    {
        auto diff = (int32_t)(next - t);
        if (diff <= 0)
        {
            next = t - (t % every) + every + at;
            return true;
        }
        return false;
    };
};

/**
 * Keep track of the state of a primitive variable and ensures that even for the first the time state is set to 'changed'
 *
 */
template <typename StateType>
class StateHolder
{
    enum
    {
        START,
        RUNNING
    } status;
    StateType state;

public:
    StateHolder(StateType start_) : status(START), state(start_) {};

    bool isChanged(StateType thisState)
    {
        auto changed = (status == START) || (state != thisState);
        status = RUNNING;
        state = thisState;
        return changed;
    }
};

template <typename T>
static void printBufferHex(etl::span<T> buffer)
{
    GATAS_ASSERT(std::is_integral<T>::value, "T must be an integral type");

    printf("Length(%d) ", static_cast<int>(buffer.size()));

    for (size_t i = 0; i < buffer.size(); ++i)
    {
        if constexpr (sizeof(T) == 1)
            printf("0x%02" PRIX8, static_cast<uint8_t>(buffer[i]));
        else if constexpr (sizeof(T) == 2)
            printf("0x%04" PRIX16, static_cast<uint16_t>(buffer[i]));
        else if constexpr (sizeof(T) == 4)
            printf("0x%08" PRIX32, static_cast<uint32_t>(buffer[i]));
        else
            printf("0x%X", static_cast<unsigned int>(buffer[i])); // fallback

        if (i + 1 < buffer.size())
            printf(", ");
    }
}

template <typename T>
static void printBufferBits(etl::span<T> buffer)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type");

    printf("Length(%d) ", static_cast<int>(buffer.size()));

    for (size_t i = 0; i < buffer.size(); ++i)
    {
        using U = std::make_unsigned_t<T>;
        U v = static_cast<U>(buffer[i]);

        constexpr int bitCount = sizeof(T) * 8;

//        printf("0b");

        for (int b = bitCount - 1; b >= 0; --b)
        {
            putchar((v & (U(1) << b)) ? '1' : '0');
        }

        // if (i + 1 < buffer.size())
        //     printf(" ");
    }

    printf("\n");
}