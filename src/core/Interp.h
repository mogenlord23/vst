#pragma once
#include <cmath>

namespace mf
{
/** 4-point, 3rd-order Hermite (Catmull-Rom) interpolation between x0 and x1, t in [0,1). */
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

/** Reads a channel at fractional position `pos` with clamped edges. */
inline float readHermite (const float* data, int length, double pos) noexcept
{
    if (length <= 0)
        return 0.0f;
    const auto ip = (int) std::floor (pos);
    const auto t = (float) (pos - (double) ip);
    auto at = [data, length] (int i) noexcept
    {
        i = i < 0 ? 0 : (i >= length ? length - 1 : i);
        return data[i];
    };
    return hermite (at (ip - 1), at (ip), at (ip + 1), at (ip + 2), t);
}
} // namespace mf
