/*
exDesktop: calm Material 3 Expressive hover for round buttons (lift + highlight, no waves).
Licensed under GPLv3, like the rest of the project.
*/
#pragma once

#include "base/basic_types.h"

namespace Ui {

class RoundButton;

// On hover the button rises a little over a soft shadow and its colour fades in; no waves.
void SetCalmHover(not_null<RoundButton*> button);

} // namespace Ui
