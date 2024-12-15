#include <stdint.h>
#include <stdio.h>

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

/**
 * Dump the buffer as HEX to the terminal
 */
//void print_buffer(uint8_t *buffer, uint8_t length);

template <typename T>
inline void printBuffer(T *buffer, uint8_t length, const char format[] = "%0X")
{
    printf("Length(%d) ", length);
    for (uint8_t i = 0; i < length; i++)
    {
        printf(format, buffer[i]);
        if (i < length - 1)
        {
            printf(" ");
        }
    }
}
