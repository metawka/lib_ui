// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/widgets/buttons.h"

#include "ui/effects/ripple_animation.h"
#include "ui/effects/cross_animation.h"
#include "ui/effects/numbers_animation.h"
#include "ui/image/image_prepare.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/checkbox.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/qt_object_factory.h"
#include "ui/round_rect.h"
#include "styles/palette.h"

#include <QtGui/QtEvents>
#include <QtGui/QPainterPath>

namespace Ui {
namespace {

class SimpleRippleButton : public RippleButton {
public:
	using RippleButton::RippleButton;

protected:
	QPoint prepareRippleStartPosition() const override final {
		const auto result = mapFromGlobal(QCursor::pos());
		return rect().contains(result)
			? result
			: DisabledRippleStartPosition();
	}

};

class SimpleCircleButton final : public SimpleRippleButton {
public:
	using SimpleRippleButton::SimpleRippleButton;

protected:
	QImage prepareRippleMask() const override final {
		return RippleAnimation::EllipseMask(size());
	}

};

class SimpleRoundButton final : public SimpleRippleButton {
public:
	using SimpleRippleButton::SimpleRippleButton;

protected:
	QImage prepareRippleMask() const override final {
		return RippleAnimation::RoundRectMask(size(), st::buttonRadius);
	}

};

[[nodiscard]] bool IsGroupedItem(QWidget *w) {
	if (const auto button = dynamic_cast<SettingsButton*>(w)) {
		return (button->st().textBg->c.alpha() == 255);
	}
	return w->property("md3GroupedRow").toBool();
}

[[nodiscard]] bool ContainsGroupedEdge(QWidget *w, bool top, int depth) {
	if (IsGroupedItem(w)) {
		return true;
	} else if (depth <= 0) {
		return false;
	}
	for (const auto child : w->children()) {
		const auto widget = qobject_cast<QWidget*>(child);
		if (!widget || !widget->isVisibleTo(w) || widget->height() <= 0) {
			continue;
		}
		const auto geometry = widget->geometry();
		const auto touches = top
			? (geometry.top() <= 0)
			: (geometry.bottom() >= w->height() - 1);
		if (touches
			&& geometry.width() * 2 >= w->width()
			&& ContainsGroupedEdge(widget, top, depth - 1)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool GroupedNeighbour(QWidget *self, bool above) {
	auto current = self;
	for (auto level = 0; level != 6; ++level) {
		const auto parent = current->parentWidget();
		if (!parent || current->isWindow()) {
			return false;
		}
		const auto geometry = current->geometry();
		auto found = (QWidget*)nullptr;
		for (const auto child : parent->children()) {
			const auto widget = qobject_cast<QWidget*>(child);
			if (!widget
				|| widget == current
				|| !widget->isVisibleTo(parent)
				|| widget->height() <= 0) {
				continue;
			}
			const auto other = widget->geometry();
			if (other.right() < geometry.left()
				|| other.left() > geometry.right()) {
				continue;
			}
			const auto adjacent = above
				? (other.bottom() + 1 == geometry.top())
				: (other.top() == geometry.bottom() + 1);
			if (adjacent) {
				found = widget;
				break;
			}
		}
		if (found) {
			return ContainsGroupedEdge(found, !above, 4);
		}
		const auto atEdge = above
			? (geometry.top() <= 0)
			: (geometry.bottom() >= parent->height() - 1);
		if (!atEdge) {
			return false;
		}
		current = parent;
	}
	return false;
}

[[nodiscard]] QPainterPath GroupedPath(
		QRectF r,
		float64 top,
		float64 bottom) {
	auto path = QPainterPath();
	path.moveTo(r.left() + top, r.top());
	path.lineTo(r.right() - top, r.top());
	path.arcTo(r.right() - 2 * top, r.top(), 2 * top, 2 * top, 90, -90);
	path.lineTo(r.right(), r.bottom() - bottom);
	path.arcTo(
		r.right() - 2 * bottom,
		r.bottom() - 2 * bottom,
		2 * bottom,
		2 * bottom,
		0,
		-90);
	path.lineTo(r.left() + bottom, r.bottom());
	path.arcTo(
		r.left(),
		r.bottom() - 2 * bottom,
		2 * bottom,
		2 * bottom,
		270,
		-90);
	path.lineTo(r.left(), r.top() + top);
	path.arcTo(r.left(), r.top(), 2 * top, 2 * top, 180, -90);
	path.closeSubpath();
	return path;
}

[[nodiscard]] QColor GroupedMix(QColor a, QColor b, float64 amount) {
	return QColor(
		int(a.red() + (b.red() - a.red()) * amount),
		int(a.green() + (b.green() - a.green()) * amount),
		int(a.blue() + (b.blue() - a.blue()) * amount));
}

} // namespace

LinkButton::LinkButton(
	QWidget *parent,
	const QString &text,
	const style::LinkButton &st)
: AbstractButton(parent)
, _st(st)
, _text(text)
, _textWidth(st.font->width(_text)) {
	resizeToText();
	setCursor(style::cur_pointer);
}

void LinkButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto &font = (isOver() ? _st.overFont : _st.font);
	const auto pen = _textFgOverride.has_value()
		? QPen(*_textFgOverride)
		: isOver()
		? _st.overColor
		: _st.color;
	p.setFont(font);
	p.setPen(pen);
	const auto left = _st.padding.left();
	const auto top = _st.padding.top() + font->ascent;
	if (width() < naturalWidth()) {
		const auto available = width() - left - _st.padding.right();
		p.drawText(left, top, font->elided(_text, available));
	} else {
		p.drawText(left, top, _text);
	}
}

void LinkButton::setText(const QString &text) {
	_text = text;
	_textWidth = _st.font->width(_text);
	accessibilityNameChanged();
	resizeToText();
	update();
}

void LinkButton::resizeToText() {
	setNaturalWidth(_st.padding.left() + _textWidth + _st.padding.right());
}

int LinkButton::resizeGetHeight(int newWidth) {
	return _st.padding.top() + _st.font->height + _st.padding.bottom();
}

void LinkButton::setColorOverride(std::optional<QColor> textFg) {
	_textFgOverride = textFg;
	update();
}

void LinkButton::onStateChanged(State was, StateChangeSource source) {
	update();
}

RippleButton::RippleButton(QWidget *parent, const style::RippleAnimation &st)
: AbstractButton(parent)
, _st(st) {
}

void RippleButton::clearState() {
	AbstractButton::clearState();
	finishAnimating();
}

void RippleButton::finishAnimating() {
	if (_ripple) {
		_ripple.reset();
		update();
	}
}

void RippleButton::setForceRippled(
		bool rippled,
		anim::type animated) {
	if (_forceRippled != rippled) {
		_forceRippled = rippled;
		if (_forceRippled) {
			_forceRippledSubscription = style::PaletteChanged(
			) | rpl::filter([=] {
				return _ripple != nullptr;
			}) | rpl::on_next([=] {
				_ripple->forceRepaint();
			});
			ensureRipple();
			if (_ripple->empty()) {
				_ripple->addFading();
			} else {
				_ripple->lastUnstop();
			}
		} else {
			if (_ripple) {
				_ripple->lastStop();
			}
			_forceRippledSubscription.destroy();
		}
	}
	if (animated == anim::type::instant && _ripple) {
		_ripple->lastFinish();
	}
	update();
}

void RippleButton::paintRipple(
		QPainter &p,
		const QPoint &point,
		const QColor *colorOverride) {
	paintRipple(p, point.x(), point.y(), colorOverride);
}

void RippleButton::paintRipple(QPainter &p, int x, int y, const QColor *colorOverride) {
	if (_ripple) {
		_ripple->paint(p, x, y, width(), colorOverride);
		if (_ripple->empty()) {
			_ripple.reset();
		}
	}
}

void RippleButton::onStateChanged(State was, StateChangeSource source) {
	update();

	auto wasDown = static_cast<bool>(was & StateFlag::Down);
	auto down = isDown();
	if (!_st.showDuration || down == wasDown || _forceRippled) {
		return;
	}

	if (down && (source == StateChangeSource::ByPress)) {
		// Start a ripple only from mouse press.
		auto position = prepareRippleStartPosition();
		if (position != DisabledRippleStartPosition()) {
			ensureRipple();
			_ripple->add(position);
		}
	} else if (!down && _ripple) {
		// Finish ripple anyway.
		_ripple->lastStop();
	}
}

void RippleButton::ensureRipple() {
	if (!_ripple) {
		_ripple = std::make_unique<RippleAnimation>(_st, prepareRippleMask(), [this] { update(); });
	}
}

QImage RippleButton::prepareRippleMask() const {
	return RippleAnimation::RectMask(size());
}

QPoint RippleButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos());
}

RippleButton::~RippleButton() = default;

FlatButton::FlatButton(
	QWidget *parent,
	const QString &text,
	const style::FlatButton &st)
: RippleButton(parent, st.ripple)
, _text(text)
, _st(st) {
	if (_st.width < 0) {
		_width = textWidth() - _st.width;
	} else if (!_st.width) {
		_width = textWidth() + _st.height - _st.font->height;
	} else {
		_width = _st.width;
	}
	resize(_width, _st.height);
}

void FlatButton::setText(const QString &text) {
	_text = text;
	accessibilityNameChanged();
	update();
}

void FlatButton::setWidth(int w) {
	_width = w;
	if (_width < 0) {
		_width = textWidth() - _st.width;
	} else if (!_width) {
		_width = textWidth() + _st.height - _st.font->height;
	}
	resize(_width, height());
}

void FlatButton::setColorOverride(std::optional<QColor> color) {
	_colorOverride = color;
	update();
}

int32 FlatButton::textWidth() const {
	return _st.font->width(_text);
}

void FlatButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);
	update();
}

void FlatButton::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	const auto inner = QRect(0, height() - _st.height, width(), _st.height);
	p.fillRect(inner, isOver() ? _st.overBgColor : _st.bgColor);

	paintRipple(p, 0, 0);

	p.setFont(isOver() ? _st.overFont : _st.font);
	p.setRenderHint(QPainter::TextAntialiasing);
	if (_colorOverride) {
		p.setPen(*_colorOverride);
	} else {
		p.setPen(isOver() ? _st.overColor : _st.color);
	}

	const auto textRect = inner.marginsRemoved(
		_textMargins
	).marginsRemoved(
		{ 0, _st.textTop, 0, 0 }
	);
	p.drawText(textRect, _text, style::al_top);
}

void FlatButton::setTextMargins(QMargins margins) {
	_textMargins = margins;
	update();
}

RoundButton::RoundButton(
	QWidget *parent,
	rpl::producer<QString> text,
	const style::RoundButton &st)
: RippleButton(parent, st.ripple)
, _textFull(std::move(text) | rpl::map(Text::WithEntities))
, _st(st)
, _roundRect(st.radius ? st.radius : st::buttonRadius, _st.textBg)
, _roundRectOver(st.radius ? st.radius : st::buttonRadius, _st.textBgOver)
, _fullRadius(st.fullRadius) {
	_textFull.value(
	) | rpl::on_next([=](const TextWithEntities &text) {
		accessibilityNameChanged();
		resizeToText(text);
	}, lifetime());
}

void RoundButton::setTextTransform(RoundButtonTextTransform transform) {
	_transform = transform;
	resizeToText(_textFull.current());
}

void RoundButton::setText(rpl::producer<QString> text) {
	_textFull = std::move(text) | rpl::map(Text::WithEntities);
}

void RoundButton::setText(rpl::producer<TextWithEntities> text) {
	_textFull = std::move(text);
}

void RoundButton::setContext(const Text::MarkedContext &context) {
	_context = context;
	resizeToText(_textFull.current());
}

void RoundButton::setNumbersText(const QString &numbersText, int numbers) {
	if (numbersText.isEmpty()) {
		_numbers.reset();
	} else {
		if (!_numbers) {
			const auto &font = _st.style.font;
			_numbers = std::make_unique<NumbersAnimation>(font, [this] {
				numbersAnimationCallback();
			});
		}
		_numbers->setText(numbersText, numbers);
	}
	resizeToText(_textFull.current());
}

void RoundButton::setWidthChangedCallback(Fn<void()> callback) {
	if (!_numbers) {
		const auto &font = _st.style.font;
		_numbers = std::make_unique<NumbersAnimation>(font, [this] {
			numbersAnimationCallback();
		});
	}
	_numbers->setWidthChangedCallback(std::move(callback));
}

void RoundButton::setBrushOverride(std::optional<QBrush> brush) {
	_brushOverride = std::move(brush);
	update();
}

void RoundButton::setRippleOverride(std::optional<QColor> color) {
	_rippleOverride = std::move(color);
	update();
}

void RoundButton::setPenOverride(std::optional<QPen> pen) {
	_penOverride = std::move(pen);
	update();
}

void RoundButton::setTextFgOverride(std::optional<QColor> textFg) {
	_textFgOverride = std::move(textFg);
	update();
}

void RoundButton::setIconOverride(const style::icon *icon) {
	_iconOverride = icon;
	update();
}

void RoundButton::finishNumbersAnimation() {
	if (_numbers) {
		_numbers->finishAnimating();
	}
}

void RoundButton::numbersAnimationCallback() {
	resizeToText(_textFull.current());
}

void RoundButton::setFullWidth(int newFullWidth) {
	_fullWidthOverride = newFullWidth;
	resizeToText(_textFull.current());
}

void RoundButton::setFullRadius(bool enabled) {
	_fullRadius = enabled;
	update();
}

void RoundButton::setCornerRadii(
		int topLeft,
		int topRight,
		int bottomLeft,
		int bottomRight) {
	_cornerRadii = { { topLeft, topRight, bottomLeft, bottomRight } };
	update();
}

void RoundButton::resizeToText(const TextWithEntities &text) {
	if (_transform == RoundButtonTextTransform::ToUpper) {
		_text.setMarkedText(
			_st.style,
			{ text.text.toUpper(), text.entities },
			kMarkupTextOptions,
			_context);
	} else {
		_text.setMarkedText(_st.style, text, kMarkupTextOptions, _context);
	}
	int innerWidth = _text.maxWidth() + addedWidth();
	if (_fullWidthOverride > 0) {
		const auto padding = _fullRadius
			? (_st.padding.left() + _st.padding.right())
			: 0;
		resize(
			_fullWidthOverride + padding,
			_st.height + _st.padding.top() + _st.padding.bottom());
	} else if (_fullWidthOverride < 0) {
		resize(
			innerWidth - _fullWidthOverride,
			_st.height + _st.padding.top() + _st.padding.bottom());
	} else if (_st.width <= 0) {
		resize(
			innerWidth - _st.width + _st.padding.left() + _st.padding.right(),
			_st.height + _st.padding.top() + _st.padding.bottom());
	} else {
		resize(
			_st.width + _st.padding.left() + _st.padding.right(),
			_st.height + _st.padding.top() + _st.padding.bottom());
	}
	setNaturalWidth(width());

	update();
}

int RoundButton::addedWidth() const {
	auto result = 0;
	if (_numbers) {
		result += (result ? _st.numbersSkip : 0) + _numbers->countWidth();
	}
	if (!_st.icon.empty() && _st.iconPosition.x() < 0) {
		result += _st.icon.width() - _st.iconPosition.x();
	}
	return result;
}

int RoundButton::contentWidth() const {
	auto result = _text.maxWidth() + addedWidth();
	if (_fullWidthOverride < 0) {
		return result;
	} else if (_fullWidthOverride > 0) {
		const auto padding = _fullRadius
			? (_st.padding.left() + _st.padding.right())
			: 0;
		const auto delta = _st.height - _st.style.font->height;
		if (_fullWidthOverride < result + delta) {
			return std::max(_fullWidthOverride - delta - padding, 1);
		}
	}
	return std::min(
		result,
		width() - _st.padding.left() - _st.padding.right());
}

void RoundButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	auto innerWidth = contentWidth();
	auto rounded = rect().marginsRemoved(_st.padding);
	if (_fullWidthOverride < 0) {
		rounded = QRect(0, rounded.top(), innerWidth - _fullWidthOverride, rounded.height());
	}
	const auto drawRect = [&](const RoundRect &rect) {
		const auto fill = myrtlrect(rounded);
		if (_fullRadius) {
			const auto radius = rounded.height() / 2;
			PainterHighQualityEnabler hq(p);
			p.setPen(_penOverride ? *_penOverride : Qt::NoPen);
			p.setBrush(_brushOverride ? *_brushOverride : rect.color()->b);
			p.drawRoundedRect(fill, radius, radius);
		} else if (_cornerRadii) {
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(_penOverride ? *_penOverride : Qt::NoPen);
			p.setBrush(_brushOverride ? *_brushOverride : rect.color()->b);
			p.drawPath(
				ComplexRoundedRectPath(
					fill,
					(*_cornerRadii)[0],
					(*_cornerRadii)[1],
					(*_cornerRadii)[2],
					(*_cornerRadii)[3]));
		} else if (_brushOverride) {
			PainterHighQualityEnabler hq(p);
			p.setPen(_penOverride ? *_penOverride : Qt::NoPen);
			p.setBrush(*_brushOverride);
			const auto radius = _st.radius ? _st.radius : st::buttonRadius;
			p.drawRoundedRect(fill, radius, radius);
		} else {
			rect.paint(p, fill);
		}
	};
	if (_penOverride && !_rippleOverride) {
		paintRipple(p, rounded.topLeft());
	}
	drawRect(_roundRect);

	auto over = isOver();
	auto down = isDown();
	if (!_brushOverride && (over || down)) {
		drawRect(_roundRectOver);
	}

	if (!_penOverride || _rippleOverride) {
		paintRipple(
			p,
			rounded.topLeft(),
			_rippleOverride ? &*_rippleOverride : nullptr);
	}

	const auto textTop = _st.padding.top() + _st.textTop;
	auto textLeft = _st.padding.left()
		+ ((width()
			- innerWidth
			- _st.padding.left()
			- _st.padding.right()) / 2);
	if (_fullWidthOverride < 0) {
		textLeft = -_fullWidthOverride / 2;
	}
	if (!_st.icon.empty() && _st.iconPosition.x() < 0) {
		textLeft += _st.icon.width() - _st.iconPosition.x();
	}
	const auto iconLeft = (_st.iconPosition.x() >= 0)
		? _st.iconPosition.x()
		: (textLeft + _st.iconPosition.x() - _st.icon.width());
	const auto iconTop = (_st.iconPosition.y() >= 0)
		? _st.iconPosition.y()
		: (textTop + _st.iconPosition.y());
	const auto widthForText = std::max(innerWidth - addedWidth(), 0);
	if (!_text.isEmpty()) {
		if (_textFgOverride) {
			p.setPen(*_textFgOverride);
		} else {
			p.setPen((over || down) ? _st.textFgOver : _st.textFg);
		}
		auto local = st::defaultTextPalette;
		local.linkFg = (over || down)
			? _st.numbersTextFgOver
			: _st.numbersTextFg;
		_text.draw(p, {
			.position = { textLeft, textTop },
			.availableWidth = widthForText,
			.palette = &local,
			.elisionLines = 1,
		});
	}
	if (_numbers) {
		textLeft += widthForText + (widthForText ? _st.numbersSkip : 0);
		p.setPen((over || down) ? _st.numbersTextFgOver : _st.numbersTextFg);
		_numbers->paint(p, textLeft, textTop, width());
	}
	if (!_st.icon.empty()) {
		const auto &current = _iconOverride
			? *_iconOverride
			: ((over || down) && !_st.iconOver.empty())
			? _st.iconOver
			: _st.icon;
		current.paint(p, QPoint(iconLeft, iconTop), width());
	}
}

QImage RoundButton::prepareRippleMask() const {
	auto innerWidth = contentWidth();
	auto rounded = style::rtlrect(rect().marginsRemoved(_st.padding), width());
	if (_fullWidthOverride < 0) {
		rounded = QRect(0, rounded.top(), innerWidth - _fullWidthOverride, rounded.height());
	}
	if (_fullRadius) {
		return RippleAnimation::RoundRectMask(
			rounded.size(),
			rounded.height() / 2);
	} else if (_cornerRadii) {
		const auto &r = *_cornerRadii;
		return RippleAnimation::MaskByDrawer(rounded.size(), false, [&](
				QPainter &p) {
			p.drawPath(
				ComplexRoundedRectPath(
					Rect(rounded.size()),
					r[0],
					r[1],
					r[2],
					r[3]));
		});
	}
	return RippleAnimation::RoundRectMask(
		rounded.size(),
		_st.radius ? _st.radius : st::buttonRadius);
}

QPoint RoundButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - QPoint(_st.padding.left(), _st.padding.top());
}

RoundButton::~RoundButton() = default;

IconButton::IconButton(QWidget *parent, const style::IconButton &st) : RippleButton(parent, st.ripple)
, _st(st) {
	resize(_st.width, _st.height);
}

const style::IconButton &IconButton::st() const {
	return _st;
}

void IconButton::setIconOverride(const style::icon *iconOverride, const style::icon *iconOverOverride) {
	_iconOverride = iconOverride;
	_iconOverrideOver = iconOverOverride;
	update();
}

void IconButton::setIconColorOverride(std::optional<QColor> colorOverride) {
	_iconColorOverride = colorOverride;
	update();
}

void IconButton::setRippleColorOverride(const style::color *colorOverride) {
	_rippleColorOverride = colorOverride;
}

float64 IconButton::iconOverOpacity() const {
	return (isDown() || forceRippled())
		? 1.
		: _a_over.value(isOver() ? 1. : 0.);
}

void IconButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	paintRipple(p, _st.rippleAreaPosition, _rippleColorOverride ? &(*_rippleColorOverride)->c : nullptr);

	const auto overIconOpacity = iconOverOpacity();
	const auto overIcon = [&] {
		if (_iconOverrideOver) {
			return _iconOverrideOver;
		} else if (!_st.iconOver.empty()) {
			return &_st.iconOver;
		} else if (_iconOverride) {
			return _iconOverride;
		}
		return &_st.icon;
	};
	const auto justIcon = [&] {
		if (_iconOverride) {
			return _iconOverride;
		}
		return &_st.icon;
	};
	const auto icon = (overIconOpacity == 1.) ? overIcon() : justIcon();
	auto position = _st.iconPosition;
	if (position.x() < 0) {
		position.setX((width() - icon->width()) / 2);
	}
	if (position.y() < 0) {
		position.setY((height() - icon->height()) / 2);
	}
	if (_iconColorOverride) {
		icon->paint(p, position, width(), *_iconColorOverride);
	} else {
		icon->paint(p, position, width());
	}
	if (overIconOpacity > 0. && overIconOpacity < 1.) {
		const auto iconOver = overIcon();
		if (iconOver != icon) {
			p.setOpacity(overIconOpacity);
			if (_iconColorOverride) {
				iconOver->paint(p, position, width(), *_iconColorOverride);
			} else {
				iconOver->paint(p, position, width());
			}
		}
	}
}

void IconButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	auto over = isOver();
	auto wasOver = static_cast<bool>(was & StateFlag::Over);
	if (over != wasOver) {
		if (_st.duration) {
			auto from = over ? 0. : 1.;
			auto to = over ? 1. : 0.;
			_a_over.start([this] { update(); }, from, to, _st.duration);
		} else {
			update();
		}
	}
}

QPoint IconButton::prepareRippleStartPosition() const {
	auto result = mapFromGlobal(QCursor::pos())
		- _st.rippleAreaPosition;
	auto rect = QRect(0, 0, _st.rippleAreaSize, _st.rippleAreaSize);
	return rect.contains(result)
		? result
		: DisabledRippleStartPosition();
}

QImage IconButton::prepareRippleMask() const {
	return RippleAnimation::EllipseMask(QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

CrossButton::CrossButton(QWidget *parent, const style::CrossButton &st) : RippleButton(parent, st.ripple)
, _st(st)
, _loadingAnimation([=](crl::time now) { return loadingCallback(now); }) {
	resize(_st.width, _st.height);
	setCursor(style::cur_pointer);
	setVisible(false);
}

bool CrossButton::loadingCallback(crl::time now) {
	const auto result = !stopLoadingAnimation(now);
	if (!result || !anim::Disabled()) {
		update();
	}
	return result;
}

void CrossButton::toggle(bool visible, anim::type animated) {
	if (_shown != visible) {
		_shown = visible;
		if (animated == anim::type::normal) {
			if (isHidden()) {
				setVisible(true);
			}
			_showAnimation.start(
				[=] { animationCallback(); },
				_shown ? 0. : 1.,
				_shown ? 1. : 0.,
				_st.duration);
		}
	}
	if (animated == anim::type::instant) {
		finishAnimating();
	}
}

void CrossButton::animationCallback() {
	update();
	if (!_showAnimation.animating()) {
		setVisible(_shown);
	}
}

void CrossButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	auto over = isOver();
	auto shown = _showAnimation.value(_shown ? 1. : 0.);
	p.setOpacity(shown);

	paintRipple(p, _st.crossPosition);

	auto loading = 0.;
	if (_loadingAnimation.animating()) {
		const auto now = crl::now();
		if (stopLoadingAnimation(now)) {
			_loadingAnimation.stop();
		} else if (anim::Disabled()) {
			CrossAnimation::paintStaticLoading(
				p,
				_st.cross,
				over ? _st.crossFgOver : _st.crossFg,
				_st.crossPosition.x(),
				_st.crossPosition.y(),
				width(),
				shown);
			return;
		} else {
			loading = ((now - _loadingAnimation.started())
				% _st.loadingPeriod) / float64(_st.loadingPeriod);
		}
	}
	CrossAnimation::paint(
		p,
		_st.cross,
		over ? _st.crossFgOver : _st.crossFg,
		_st.crossPosition.x(),
		_st.crossPosition.y(),
		width(),
		shown,
		loading);
}

bool CrossButton::stopLoadingAnimation(crl::time now) {
	if (!_loadingStopMs) {
		return false;
	}
	const auto stopPeriod = (_loadingStopMs - _loadingAnimation.started())
		/ _st.loadingPeriod;
	const auto currentPeriod = (now - _loadingAnimation.started())
		/ _st.loadingPeriod;
	if (currentPeriod != stopPeriod) {
		Assert(currentPeriod > stopPeriod);
		return true;
	}
	return false;
}

void CrossButton::setLoadingAnimation(bool enabled) {
	if (enabled) {
		_loadingStopMs = 0;
		if (!_loadingAnimation.animating()) {
			_loadingAnimation.start();
		}
	} else if (_loadingAnimation.animating()) {
		_loadingStopMs = crl::now();
		if (!((_loadingStopMs - _loadingAnimation.started())
			% _st.loadingPeriod)) {
			_loadingAnimation.stop();
		}
	}
	if (anim::Disabled()) {
		update();
	}
}

void CrossButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	auto over = isOver();
	auto wasOver = static_cast<bool>(was & StateFlag::Over);
	if (over != wasOver) {
		update();
	}
}

QPoint CrossButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - _st.crossPosition;
}

QImage CrossButton::prepareRippleMask() const {
	return RippleAnimation::EllipseMask(QSize(_st.cross.size, _st.cross.size));
}

SettingsButton::SettingsButton(
	QWidget *parent,
	rpl::producer<QString> &&text,
	const style::SettingsButton &st)
: SettingsButton(parent, std::move(text) | rpl::map([=](QString &&text) {
	return TextWithEntities{ std::move(text) };
}), st) {
}

SettingsButton::SettingsButton(
	QWidget *parent,
	rpl::producer<TextWithEntities> &&text,
	const style::SettingsButton &st,
	const Text::MarkedContext &context)
: RippleButton(parent, st.ripple)
, _st(st)
, _padding(_st.padding)
, _context(context) {
	std::move(
		text
	) | rpl::on_next([this](TextWithEntities &&value) {
		setText(std::move(value));
	}, lifetime());
}

SettingsButton::SettingsButton(
	QWidget *parent,
	std::nullptr_t,
	const style::SettingsButton &st)
: RippleButton(parent, st.ripple)
, _st(st)
, _padding(_st.padding) {
}

SettingsButton::~SettingsButton() = default;

void SettingsButton::finishAnimating() {
	if (_toggle) {
		_toggle->finishAnimating();
	}
	Ui::RippleButton::finishAnimating();
}

SettingsButton *SettingsButton::toggleOn(
		rpl::producer<bool> &&toggled,
		bool ignoreClick) {
	Expects(_toggle == nullptr);

	_toggle = std::make_unique<Ui::ToggleView>(
		isOver() ? _st.toggleOver : _st.toggle,
		false,
		[this] { rtlupdate(toggleRect()); });
	if (!ignoreClick) {
		addClickHandler([this] {
			_toggle->setChecked(!_toggle->checked(), anim::type::normal);
		});
	}
	std::move(
		toggled
	) | rpl::on_next([this](bool toggled) {
		_toggle->setChecked(toggled, anim::type::normal);
	}, lifetime());
	_toggle->checkedChanges() | rpl::on_next([this] {
		accessibilityStateChanged({ .checked = true });
	}, lifetime());
	_toggle->finishAnimating();
	if (_allowTextLines != 1) {
		resizeToWidth(widthNoMargins());
	}
	return this;
}

bool SettingsButton::toggled() const {
	return _toggle ? _toggle->checked() : false;
}

AccessibilityState SettingsButton::accessibilityState() const {
	if (!_toggle) {
		return {};
	}
	return { .checkable = true, .checked = _toggle->checked() };
}

void SettingsButton::setToggleLocked(bool locked) {
	if (_toggle) {
		_toggle->setLocked(locked);
	}
}

rpl::producer<bool> SettingsButton::toggledChanges() const {
	if (_toggle) {
		return _toggle->checkedChanges();
	}
	return nullptr;
}

rpl::producer<bool> SettingsButton::toggledValue() const {
	if (_toggle) {
		return _toggle->checkedValue();
	}
	return nullptr;
}

void SettingsButton::setColorOverride(
		std::optional<QColor> textColorOverride) {
	_textColorOverride = textColorOverride;
	update();
}

void SettingsButton::setPaddingOverride(style::margins padding) {
	_padding = padding;
	resizeToWidth(widthNoMargins());
}

void SettingsButton::setAllowTextLines(int lines) {
	const auto wrapChanged = ((_allowTextLines == 1) != (lines == 1));
	_allowTextLines = lines;
	if (wrapChanged) {
		// An export of _text would lose colorized parts and custom objects.
		auto source = _textSource;
		setText(std::move(source));
	}
	resizeToWidth(widthNoMargins());
	update();
}

const style::SettingsButton &SettingsButton::st() const {
	return _st;
}

int SettingsButton::fullTextWidth() const {
	return _text.maxWidth();
}

QPainterPath PaintGroupedRow(
		QPainter &p,
		not_null<QWidget*> self,
		QColor base,
		bool over) {
	const auto inset = style::ConvertScale(10);
	const auto gap = std::max(style::ConvertScale(1), 1);
	const auto rp = dynamic_cast<RpWidget*>(self.get());
	const auto extra = rp ? rp->getMargins() : QMargins();
	const auto width = self->width() - extra.left() - extra.right();
	const auto height = self->height();
	const auto prev = GroupedNeighbour(self, true);
	const auto next = GroupedNeighbour(self, false);
	const auto big = style::ConvertScale(18.);
	const auto small = style::ConvertScale(4.);
	const auto card = QRectF(
		extra.left() + inset,
		prev ? gap : 0,
		width - 2 * inset,
		height - (prev ? gap : 0) - (next ? gap : 0));
	const auto path = GroupedPath(
		card,
		prev ? small : big,
		next ? small : big);
	auto hq = PainterHighQualityEnabler(p);
	p.fillPath(
		path,
		GroupedMix(base, st::windowFg->c, over ? 0.09 : 0.055));
	return path;
}

void SetupGroupedRowBackground(not_null<RpWidget*> row) {
	if (!style::Md3Grouped()) {
		return;
	}
	row->setProperty("md3GroupedRow", true);
	const auto parent = row->parentWidget();
	if (!parent) {
		return;
	}
	const auto back = CreateChild<RpWidget>(parent);
	back->setProperty("md3GroupedRow", true);
	back->setAttribute(Qt::WA_TransparentForMouseEvents);
	back->stackUnder(row);
	row->geometryValue() | rpl::on_next([=](QRect geometry) {
		back->setGeometry(geometry);
		back->update();
	}, back->lifetime());
	row->shownValue() | rpl::on_next([=](bool shown) {
		back->setVisible(shown);
	}, back->lifetime());
	back->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(back);
		PaintGroupedRow(p, row, st::windowBg->c, false);
	}, back->lifetime());
	QObject::connect(row.get(), &QObject::destroyed, back, [=] {
		delete back;
	});
}

void SettingsButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto paintOver = (isOver() || isDown()) && !isDisabled();
	const auto inset = style::ConvertScale(10);
	const auto grouped = style::Md3Grouped()
		&& (_st.textBg->c.alpha() == 255);
	if (grouped && width() <= inset * 20) {
		paintRipple(p, 0, 0);
	} else if (grouped) {
		const auto path = PaintGroupedRow(p, this, _st.textBg->c, paintOver);
		p.save();
		p.setClipPath(path);
		paintRipple(p, 0, 0);
		p.restore();
	} else if (_st.textBg->c.alpha() == 0 && st::menuItemBgRadius > 0) {
		const auto margins = QMargins(
			st::menuItemBgMargins.left() + inset / 2,
			st::menuItemBgMargins.top(),
			st::menuItemBgMargins.right() + inset / 2,
			st::menuItemBgMargins.bottom());
		const auto pill = QRectF(rect().marginsRemoved(margins));
		const auto radius = pill.height() / 2.;
		auto path = QPainterPath();
		path.addRoundedRect(pill, radius, radius);
		if (paintOver) {
			auto hq = PainterHighQualityEnabler(p);
			p.fillPath(path, _st.textBgOver);
		}
		p.save();
		p.setClipPath(path);
		paintRipple(p, 0, 0);
		p.restore();
	} else {
		paintBg(p, e->rect(), paintOver);
		paintRipple(p, 0, 0);
	}

	const auto outerw = width();
	paintText(p, paintOver, outerw);

	if (_toggle) {
		paintToggle(p, outerw);
	}
}

void SettingsButton::paintBg(Painter &p, const QRect &rect, bool over) const {
	p.fillRect(rect, over ? _st.textBgOver : _st.textBg);
}

void SettingsButton::paintText(Painter &p, bool over, int outerw) const {
	const auto available = textAvailableWidth(outerw);
	if (available <= 0) {
		return;
	}
	p.setPen(_textColorOverride
		? QPen(*_textColorOverride)
		: over
		? _st.textFgOver
		: _st.textFg);
	_text.drawLeftElided(
		p,
		_padding.left(),
		_padding.top(),
		available,
		outerw,
		_allowTextLines);
}

int SettingsButton::textAvailableWidth(int outerw) const {
	return outerw
		- _padding.left()
		- _padding.right()
		- (_toggle ? (_st.toggleSkip + _toggle->getSize().width()) : 0);
}

void SettingsButton::paintToggle(Painter &p, int outerw) const {
	if (_toggle) {
		auto rect = toggleRect();
		_toggle->paint(p, rect.left(), rect.top(), outerw);
	}
}

QRect SettingsButton::toggleRect() const {
	Expects(_toggle != nullptr);

	auto size = _toggle->getSize();
	auto left = width() - _st.toggleSkip - size.width();
	auto top = (height() - size.height()) / 2;
	return { QPoint(left, top), size };
}

QRect SettingsButton::maybeToggleRect() const {
	return _toggle ? toggleRect() : QRect(0, 0, 0, 0);
}

int SettingsButton::resizeGetHeight(int newWidth) {
	const auto single = _padding.top() + _st.height + _padding.bottom();
	const auto available = textAvailableWidth(newWidth);
	if (_allowTextLines == 1 || available <= 0 || _text.isEmpty()) {
		return single;
	}
	const auto height = _text.countDimensions(Text::SimpleGeometry(
		available,
		_allowTextLines,
		0,
		false)).height;
	return std::max(single, single - _text.lineHeight() + height);
}

void SettingsButton::onStateChanged(
		State was,
		StateChangeSource source) {
	const auto wasDisabled = !!(was & StateFlag::Disabled);
	const auto nowDisabled = isDisabled();
	if (!nowDisabled || !isDown()) {
		RippleButton::onStateChanged(was, source);
	}
	if (_toggle) {
		_toggle->setStyle(isOver() ? _st.toggleOver : _st.toggle);
	}
	if (nowDisabled != wasDisabled) {
		setPointerCursor(!isDisabled());
	}
}

void SettingsButton::setText(TextWithEntities &&text) {
	const auto minResizeWidth = (_allowTextLines == 1) ? kQFixedMax : 1;
	if (_text.minResizeWidth() != minResizeWidth) {
		_text = Text::String(minResizeWidth);
	}
	_textSource = std::move(text);
	_text.setMarkedText(_st.style, _textSource, kMarkupTextOptions, _context);
	accessibilityNameChanged();
	if (_allowTextLines != 1) {
		resizeToWidth(widthNoMargins());
	}
	update();
}

not_null<RippleButton*> CreateSimpleRectButton(
		QWidget *parent,
		const style::RippleAnimation &st) {
	const auto result = CreateChild<SimpleRippleButton>(parent, st);
	result->paintRequest() | rpl::on_next([result] {
		auto p = QPainter(result);
		result->paintRipple(p, 0, 0);
	}, result->lifetime());
	return result;
}

not_null<RippleButton*> CreateSimpleSettingsButton(
		QWidget *parent,
		const style::RippleAnimation &st,
		const style::color &bg) {
	const auto result = CreateChild<SimpleRippleButton>(parent, st);
	result->paintRequest() | rpl::on_next([result, bg] {
		auto p = QPainter(result);
		const auto paintOver = (result->isOver() || result->isDown())
			&& !result->isDisabled();
		if (paintOver) {
			p.fillRect(result->rect(), bg);
		}
		result->paintRipple(p, 0, 0);
	}, result->lifetime());
	return result;
}

not_null<RippleButton*> CreateSimpleCircleButton(
		QWidget *parent,
		const style::RippleAnimation &st) {
	const auto result = CreateChild<SimpleCircleButton>(parent, st);
	result->paintRequest() | rpl::on_next([result] {
		auto p = QPainter(result);
		result->paintRipple(p, 0, 0);
	}, result->lifetime());
	return result;
}

not_null<RippleButton*> CreateSimpleRoundButton(
		QWidget *parent,
		const style::RippleAnimation &st) {
	const auto result = CreateChild<SimpleRoundButton>(parent, st);
	result->paintRequest() | rpl::on_next([result] {
		auto p = QPainter(result);
		result->paintRipple(p, 0, 0);
	}, result->lifetime());
	return result;
}

} // namespace Ui
