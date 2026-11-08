// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/widgets/box_content_divider.h"
#include "ui/painter.h"

#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "styles/palette.h"

#include <QtGui/QPainter>
#include <QtGui/QtEvents>

namespace Ui {

BoxContentDivider::BoxContentDivider(
	QWidget *parent,
	int height,
	const style::DividerBar &st,
	RectParts parts)
: RpWidget(parent)
, _st(st)
, _parts(parts) {
	resize(width(), height);
}

void BoxContentDivider::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	// Material 3: no full-width grey bands. A thin gap becomes a hairline divider, a taller
	// block (usually with explanatory text over it) becomes a rounded tonal card.
	for (auto w = parentWidget(); w; w = w->parentWidget()) {
		if (w->property("exdCards").toBool()) {
			return; // the settings cards draw the sections, the divider is just a gap
		}
	}
	p.fillRect(e->rect(), st::windowBg);
	PainterHighQualityEnabler hq(p);
	if (height() <= style::ConvertScale(20)) {
		const auto inset = style::ConvertScaleExact(24.);
		const auto y = height() / 2.;
		p.setPen(QPen(st::boxDividerFg, style::ConvertScaleExact(1.)));
		p.drawLine(QPointF(inset, y), QPointF(width() - inset, y));
	} else {
		const auto card = QRectF(rect()).marginsRemoved(QMarginsF(
			style::ConvertScaleExact(12.),
			style::ConvertScaleExact(6.),
			style::ConvertScaleExact(12.),
			style::ConvertScaleExact(6.)));
		const auto radius = style::ConvertScaleExact(16.);
		p.setPen(Qt::NoPen);
		p.setBrush(_st.bg);
		p.drawRoundedRect(card, radius, radius);
	}
}

void BoxContentDivider::paintTop(QPainter &p, int skip) {
	const auto dividerFillTop = QRect(
		0,
		skip,
		width(),
		_st.top.height());
	_st.top.fill(p, dividerFillTop);
}

void BoxContentDivider::paintBottom(QPainter &p, int skip) {
	const auto dividerFillBottom = myrtlrect(
		0,
		height() - skip - _st.bottom.height(),
		width(),
		_st.bottom.height());
	_st.bottom.fill(p, dividerFillBottom);
}

const style::color &BoxContentDivider::color() const {
	return _st.bg;
}

} // namespace Ui
