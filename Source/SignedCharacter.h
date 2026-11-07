#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// One topology, continuously interpolated independently in signed screen XY.
// Screen y<0 is top. Radial strength and the established neutral gate are untouched.
struct SignedCharacter
{
    float bitsOffset = 0, logRate = 0, shaper = 0, antiAlias = 0, rebuild = 0, warmth = 0;
    static SignedCharacter at(float x, float y, float slope=2) noexcept
    {
        x = std::tanh(slope * std::clamp(x, -1.f, 1.f));
        y = std::tanh(slope * std::clamp(y, -1.f, 1.f));
        const std::array<float, 4> w{(1 - x) * (1 - y) * .25f, (1 + x) * (1 - y) * .25f,
                                     (1 - x) * (1 + y) * .25f, (1 + x) * (1 + y) * .25f};
        // TL crunch, TR metallic, BL warmer grit, BR robotic sample-and-hold.
        constexpr std::array<std::array<float, 6>, 4> corners{{{-1.8f, .14f, .65f, .38f, .26f, .08f},
                                                               {-.5f, -.3f, 1.f, .08f, .08f, 0.f},
                                                               {1.6f, .3f, .25f, .78f, .72f, .48f},
                                                               {.4f, -.75f, .4f, .12f, .025f, .05f}}};
        SignedCharacter c;
        float *fields[]{&c.bitsOffset, &c.logRate, &c.shaper, &c.antiAlias, &c.rebuild, &c.warmth};
        for (std::size_t j = 0; j < 6; ++j)
            for (std::size_t i = 0; i < 4; ++i)
                *fields[j] += w[i] * corners[i][j];
        return c;
    }
};
