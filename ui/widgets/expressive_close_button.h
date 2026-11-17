/*
exDesktop: Material 3 Expressive close button for dialogs.
Licensed under GPLv3, like the rest of the project.
*/
#pragma once

#include "ui/widgets/buttons.h"

namespace Ui {

// A close button that lives: on hover a tonal container grows out of the circle and turns
// into a slowly turning lobed shape with a wavy outline, and the cross spins half a turn with
// a spring. A press calms the waves and squeezes it; leaving fades everything back smoothly.
class ExpressiveCloseButton final : public IconButton {
public:
	ExpressiveCloseButton(QWidget *parent, const style::IconButton &st);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

private:
	Ui::Animations::Simple _hover;
	Ui::Animations::Simple _press;
	Ui::Animations::Basic _clock;
	double _phase = 0.;

};

} // namespace Ui
