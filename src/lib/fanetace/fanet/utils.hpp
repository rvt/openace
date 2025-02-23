#pragma once

#include <stdint.h>
#include <etl/math.h>
#include <etl/power.h>
#include <etl/ratio.h>
#include <etl/algorithm.h>

namespace FANET
{
    
    template <typename R, typename UnitFactor, typename ScalingFactor, uint8_t bitCount>
    auto toScaled(float number)
    {
        struct Result
        {
            R value;
            bool scaled;
        };
    
        // Compute constants
        constexpr float unitFactor = static_cast<float>(UnitFactor::num) / UnitFactor::den;
        constexpr float scalingFactor = static_cast<float>(ScalingFactor::num) / ScalingFactor::den;
        constexpr uint8_t maxBits = bitCount - static_cast<uint8_t>(std::is_signed<R>::value);
        constexpr int32_t constrainedMax = (1 << maxBits) - 1;
    
        // Ensure non-negative values for unsigned types
        if constexpr (etl::is_unsigned<R>::value)
        {
            number = etl::max(0.0f, number);
        }
    
        float ret = roundf(number / unitFactor);    

        // Check if the number fits without scaling
        if (etl::absolute(ret) <= constrainedMax)
        {
            return Result{
                etl::clamp(static_cast<R>(ret), static_cast<R>(etl::is_unsigned<R>::value ? 0 : -constrainedMax), static_cast<R>(constrainedMax)),
                false};
        }

        // Scale and clamp the result
        float scaled = roundf(number / scalingFactor);
        
        return Result{
            etl::clamp(static_cast<R>(scaled), static_cast<R>(etl::is_unsigned<R>::value ? 0 : -constrainedMax), static_cast<R>(constrainedMax)),
            true};
    }
    
}
