#pragma once
template <typename T>

/**
 * @brief Simple wrapper that keeps track if a value has been modified
 * this would avoid static or other booleans to keep track of changes
 * Note:Need to re-think this volatile thingy I had...
 * 
 * Only the Setter will set the ditry flag.
 * 
 */
class Property
{
public:
    Property() = default;
    Property(const T &initial) : _value(initial) {}

    // Assignment operator
    Property &operator=(const T &newValue)
    {
        _modified = _value != newValue;
        _value = newValue;
        return *this;
    }

    void set(const T &newValue)
    {
        _modified = _value != newValue;
        _value = newValue;
    }

    void set(const T &newValue) volatile
    {
        _modified = _value != newValue;
        _value = newValue;
    }

    // Get value with implicit conversion
    operator T() const
    {
        return _value;
    }

    // Explicit getter
    T value() const
    {
        return _value;
    }

    T value() const volatile
    {
        return _value;
    }

    // Modified flag
    bool isModified() const
    {
        return _modified;
    }

    bool isModified() const volatile
    {
        return _modified;
    }

private:
    T _value{};
    bool _modified = false;
};
