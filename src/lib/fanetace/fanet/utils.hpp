#pragma once

#include <stdint.h>
#include <etl/math.h>
#include <etl/power.h>
#include <etl/ratio.h>
#include <etl/algorithm.h>

namespace FANET
{
    /**
     * @brief Scales a floating-point number to a fixed-point representation.
     * 
     * This function scales a floating-point number to a fixed-point representation
     * based on the provided unit factor, scaling factor, and bit count. It ensures
     * that the scaled value fits within the specified bit count and clamps the result
     * if necessary.
     * 
     * @tparam R The return type (fixed-point representation).
     * @tparam UnitFactor The unit factor for scaling.
     * @tparam ScalingFactor The scaling factor for scaling.
     * @tparam bitCount The number of bits for the fixed-point representation.
     * @param number The floating-point number to scale.
     * @return A struct containing the scaled value and a boolean indicating if scaling was applied.
     */
    template <typename R, typename UnitFactor, typename ScalingFactor, uint8_t bitCount>
    auto toScaled(float number)
    {
        struct Result
        {
            R value;   ///< The scaled value.
            bool scaled; ///< Indicates if scaling was applied.
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
