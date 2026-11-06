#pragma once
#include <algorithm>

// Logical JUCE dimensions. JUCE/WebView2 apply Windows monitor DPI scaling;
// do not multiply these by the display scale a second time.
namespace WubrotGeometry
{
constexpr int designWidth = 1200, designHeight = 1040;
inline int width(int index) noexcept
{
    constexpr int widths[] { 600, 804, 996 };
    return widths[std::clamp(index, 0, 2)];
}
inline int height(int index) noexcept
{
    return (width(index) * designHeight + designWidth - 1) / designWidth;
}
}
