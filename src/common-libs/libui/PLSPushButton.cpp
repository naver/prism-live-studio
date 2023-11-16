#include "PLSPushButton.h"
#include <QPainter>

void PLSPushButton::resizeEvent(QResizeEvent *event)
{
	QPushButton::setText(GetNameElideString());

	QPushButton::resizeEvent(event);
}

QString PLSPushButton::GetNameElideString() const
{
	QFontMetrics fontWidth(this->font());
	if (fontWidth.horizontalAdvance(text) > this->width())
		return fontWidth.elidedText(text, Qt::ElideRight, this->width());
	return text;
}

void PLSPushButton::setText(const QString &_text)
{
	text = _text;
	QPushButton::setText(GetNameElideString());
}

PLSIconButton::PLSIconButton(QWidget *parent) : QPushButton(parent)
{
	setMouseTracking(true);
}

QString PLSIconButton::iconOffNormal() const
{
	return m_icons[PLSUiApp::UncheckedNormal].first;
}

void PLSIconButton::setIconOffNormal(const QString &icon)
{
	setIconFile(icon, PLSUiApp::UncheckedNormal);
}

QString PLSIconButton::iconOffHover() const
{
	return m_icons[PLSUiApp::UncheckedHover].first;
}

void PLSIconButton::setIconOffHover(const QString &icon)
{
	setIconFile(icon, PLSUiApp::UncheckedHover);
}

QString PLSIconButton::iconOffPressed() const
{
	return m_icons[PLSUiApp::UncheckedPressed].first;
}

void PLSIconButton::setIconOffPressed(const QString &icon)
{
	setIconFile(icon, PLSUiApp::UncheckedPressed);
}

QString PLSIconButton::iconOffDisabled() const
{
	return m_icons[PLSUiApp::UncheckedDisabled].first;
}

void PLSIconButton::setIconOffDisabled(const QString &icon)
{
	setIconFile(icon, PLSUiApp::UncheckedDisabled);
}

QString PLSIconButton::iconOnNormal() const
{
	return m_icons[PLSUiApp::CheckedNormal].first;
}

void PLSIconButton::setIconOnNormal(const QString &icon)
{
	setIconFile(icon, PLSUiApp::CheckedNormal);
}

QString PLSIconButton::iconOnHover() const
{
	return m_icons[PLSUiApp::CheckedHover].first;
}

void PLSIconButton::setIconOnHover(const QString &icon)
{
	setIconFile(icon, PLSUiApp::CheckedHover);
}

QString PLSIconButton::iconOnPressed() const
{
	return m_icons[PLSUiApp::CheckedPressed].first;
}

void PLSIconButton::setIconOnPressed(const QString &icon)
{
	setIconFile(icon, PLSUiApp::CheckedPressed);
}

QString PLSIconButton::iconOnDisabled() const
{
	return m_icons[PLSUiApp::CheckedDisabled].first;
}

void PLSIconButton::setIconOnDisabled(const QString &icon)
{
	setIconFile(icon, PLSUiApp::CheckedDisabled);
}

int PLSIconButton::iconWidth() const
{
	return m_iconWidth;
}

void PLSIconButton::setIconWidth(int width)
{
	m_iconWidth = width;
}

int PLSIconButton::iconHeight() const
{
	return m_iconHeight;
}

void PLSIconButton::setIconHeight(int height)
{
	m_iconHeight = height;
}

int PLSIconButton::marginLeft() const
{
	return m_marginLeft;
}

void PLSIconButton::setMarginLeft(int margin) 
{
	m_marginLeft = margin;
	update();
}

int PLSIconButton::marginRight() const
{
	return m_marginRight;
}

void PLSIconButton::setMarginRight(int margin) 
{
	m_marginRight = margin;
	update();
}

void PLSIconButton::setIconFile(const QString &icon, PLSUiApp::Icon type)
{
	m_icons[type].first = icon;
	m_icons[type].second = pls_load_pixmap(icon, QSize(m_iconWidth, m_iconHeight) * 4);
	update();
}

bool PLSIconButton::event(QEvent *event)
{
	bool showMode = property("showMode").toBool();
	switch (event->type()) {
	case QEvent::EnabledChange:
		update();
		break;
	case QEvent::Enter:
		if (isEnabled()) {
			m_hoverd = true;
			m_state = showMode ? PLSUiApp::CheckedHover : PLSUiApp::UncheckedHover;
		}
		break;
	case QEvent::Leave:
		if (isEnabled()) {
			m_hoverd = false;
			m_state = showMode ? PLSUiApp::CheckedNormal : PLSUiApp::UncheckedNormal;
		}
		break;
	case QEvent::MouseButtonPress:
		if (isEnabled())
			m_state = showMode ? PLSUiApp::CheckedPressed : PLSUiApp::UncheckedPressed;
		break;
	case QEvent::MouseButtonRelease:
		if (isEnabled()) {
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				m_state = showMode ? PLSUiApp::CheckedHover : PLSUiApp::UncheckedHover;
			} else {
				m_state = showMode ? PLSUiApp::CheckedNormal : PLSUiApp::UncheckedNormal;
			}
		}
		break;
	case QEvent::MouseMove:
		if (isEnabled()) {
			m_hoverd = true;
			m_state = showMode ? PLSUiApp::CheckedHover : PLSUiApp::UncheckedHover;
		}
		break;
	case QEvent::DynamicPropertyChange:
		if (isEnabled()) {
			auto e = dynamic_cast<QDynamicPropertyChangeEvent *>(event);
			if (e->propertyName() == "showMode") {
				m_state = showMode ? (m_hoverd ? PLSUiApp::CheckedHover : PLSUiApp::CheckedNormal) : (m_hoverd ? PLSUiApp::UncheckedHover : PLSUiApp::UncheckedNormal);
			}
		}
		break;
	default:
		break;
	}

	return QWidget::event(event);
}

void PLSIconButton::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	if (const auto &icon = m_icons[m_state].second; !icon.isNull()) {
		QPixmap pix = icon.scaled(QSize(m_iconWidth, m_iconHeight) * devicePixelRatio(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

		QRect rc = rect();
		painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
		painter.drawPixmap(QRect(m_marginRight, 1, width() - m_marginRight - m_marginLeft, height() - 2), pix);
	}
}

PLSSwitchButton::PLSSwitchButton(QWidget *parent) : PLSIconButton(parent)
{
	setProperty("showMode", false);
}

PLSSwitchButton::~PLSSwitchButton() {}

void PLSSwitchButton::setChecked(bool checked)
{
	bool changed = (m_check != checked);
	m_check = checked;
	m_checkState = m_check ? Qt::Checked : Qt::Unchecked;
	if (changed) {
		setProperty("showMode", checked);
		update();
		emit stateChanged(m_checkState);
	}
}

bool PLSSwitchButton::isChecked() const
{
	return m_check;
}

void PLSSwitchButton::paintEvent(QPaintEvent *event) 
{
	auto app = PLSUiApp::instance();
	if (!app)
		return;

	QPainter painter(this);

	if (const auto &icon = app->m_switchButtonIcons[m_state]; !icon.isNull()) {
		QPixmap pix = icon.scaled(QSize(m_iconWidth, m_iconHeight) * devicePixelRatio(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

		QRect rc = rect();
		painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
		painter.drawPixmap(QRect(m_marginLeft, 1, width() - m_marginRight - m_marginLeft, height() - 2), pix);
	}
}

bool PLSSwitchButton::event(QEvent *event)
{
	auto result = PLSIconButton::event(event);

	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		if (isEnabled()) {
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				setChecked(!m_check);
			}
		}
		break;
	default:
		break;
	}

	return result;
}
