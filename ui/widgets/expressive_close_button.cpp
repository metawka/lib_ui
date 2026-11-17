/*
exDesktop: Material 3 Expressive close button for dialogs.
Licensed under GPLv3, like the rest of the project.
*/
#include "ui/widgets/expressive_close_button.h"

#include "ui/painter.h"
#include "styles/palette.h"
#include "styles/style_basic.h"

#include <QtGui/QPainterPath>
#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>

namespace Ui {
namespace {

QColor Mix(const QColor &a, const QColor &b, double t) {
	t = std::clamp(t, 0., 1.);
	return QColor::fromRgbF(
		a.redF() + (b.redF() - a.redF()) * t,
		a.greenF() + (b.greenF() - a.greenF()) * t,
		a.blueF() + (b.blueF() - a.blueF()) * t,
		a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// r(a) = R * (1 + depth * cos(lobes * a + turn)): a circle that turns into a soft flower
QPainterPath Lobed(QPointF center, double radius, double depth, int lobes, double turn) {
	auto path = QPainterPath();
	constexpr auto kSteps = 180;
	for (auto i = 0; i <= kSteps; ++i) {
		const auto a = 2. * M_PI * i / kSteps;
		const auto r = radius * (1. + depth * std::cos(lobes * a + turn));
		const auto point = center + QPointF(std::cos(a) * r, std::sin(a) * r);
		(i == 0) ? path.moveTo(point) : path.lineTo(point);
	}
	path.closeSubpath();
	return path;
}

} // namespace

ExpressiveCloseButton::ExpressiveCloseButton(
	QWidget *parent,
	const style::IconButton &st)

: IconButton(parent, st) {
	_clock.init([=](crl::time now) {
		_phase = now / 1000. * 4.4;
		update();
		return isOver() || _hover.value(0.) > 0.001;
	});
}

void ExpressiveCloseButton::onStateChanged(State was, StateChangeSource source) {
	IconButton::onStateChanged(was, source);
	const auto over = isOver();
	if (over != static_cast<bool>(was & StateFlag::Over)) {
		_hover.start(
			[=] { update(); },
			_hover.value(over ? 0. : 1.),
			over ? 1. : 0.,
			over ? 500 : 420,
			over ? anim::easeOutBack : anim::easeOutCubic);
		if (over && !_clock.animating()) {
			_clock.start();
		}
	}
	const auto down = isDown();
	if (down != static_cast<bool>(was & StateFlag::Down)) {
		_press.start(
			[=] { update(); },
			_press.value(down ? 0. : 1.),
			down ? 1. : 0.,
			down ? 110 : 420,
			down ? anim::sineInOut : anim::easeOutBack);
	}
}

void ExpressiveCloseButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	const auto k = std::max(0., _hover.value(isOver() ? 1. : 0.));
	const auto shown = std::min(1., k);
	const auto press = std::clamp(_press.value(isDown() ? 1. : 0.), 0., 1.);
	// the container is centred in the button's width (a layer's close button is only 48 px wide)
	// and sits at the height of its round ripple area
	const auto &style = st();
	const auto centerY = (style.rippleAreaSize > 0)
		? style.rippleAreaPosition.y() + style.rippleAreaSize / 2.
		: rect().center().y() + 0.5;
	const auto center = QPointF(rect().width() / 2., centerY);
	const auto one = style::ConvertScaleExact(1.);

	// the tonal container: grows from a small circle, becomes a lobed flower, calms on press
	// big enough to read, small enough that the ring still fits (with the spring's overshoot)
	const auto fitting = std::min<double>(rect().width(), rect().height()) / 2. - style::ConvertScaleExact(7.5);
	const auto base = std::min(style::ConvertScaleExact(20.), fitting);
	const auto scale = (0.72 + 0.28 * k) * (1. - 0.1 * press);
	const auto depth = 0.075 * shown * (1. - 0.8 * press);
	if (shown > 0.001) {
		auto fill = st::lightButtonBg->c;
		fill.setAlphaF(fill.alphaF() * std::min(1., shown * 1.3));
		p.setPen(Qt::NoPen);
		p.setBrush(fill);
		p.drawPath(Lobed(center, base * scale, depth, 8, _phase));

		// the wavy outline, interleaved with the container's lobes and turning the other way
		auto ring = st::windowBgActive->c;
		ring.setAlphaF(ring.alphaF() * 0.85 * std::min(1., shown * 1.2));
		auto pen = QPen(ring, 1.6 * one, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		const auto ringRadius = base * scale + (3.2 + 1.0 * k) * one;
		p.drawPath(Lobed(center, ringRadius, 0.05 * shown * (1. - 0.8 * press), 8, -_phase * 1.3 + M_PI / 8.));
	}

	// the cross: half a turn with a spring, a little larger while hovered
	const auto color = Mix(st::boxTitleCloseFg->c, st::lightButtonFg->c, shown);
	const auto arm = style::ConvertScaleExact(5.6) * (1. + 0.08 * k) * (1. - 0.12 * press);
	p.save();
	p.translate(center);
	p.rotate(180. * k);
	p.setPen(QPen(color, 2. * one, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(QPointF(-arm, -arm), QPointF(arm, arm));
	p.drawLine(QPointF(-arm, arm), QPointF(arm, -arm));
	p.restore();
}

} // namespace Ui
