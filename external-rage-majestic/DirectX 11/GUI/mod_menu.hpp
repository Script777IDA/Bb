#pragma once

#include <cstddef>

namespace mod_menu {

// Renders the native ImGui version of the supplied HTML menu.
// The controls are local UI state; feature execution is intentionally kept
// outside this renderer so the menu can be integrated without embedding
// feature-specific behavior into the presentation layer.
void initialize();
void render(bool& menu_open);
int menu_key();

} // namespace mod_menu
