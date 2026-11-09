#pragma once
#include "Graphics/Graphics.h"

namespace CustomCursor::Seasonal::Ambient {

// Animated particles over the whole screen (bats / falling leaves, bubbles, snowflakes).
void Draw(Graphics& g, const Rect& screen, double time);

} // namespace CustomCursor::Seasonal::Ambient
