#include "PLSCheckBox.h"
#include "PLSUIApp.h"

#include <qevent.h>
#include <qlabel.h>
#include <qboxlayout.h>
#include <qpainter.h>

#include <libui.h>
#include <libutils-api.h>

constexpr QSize ICON_SIZE{15, 15};
constexpr int FACTOR = 4;

PLSCheckBox::PLSCheckBox(QWidget *parent) : PLSCheckBox(QString(), parent) {}
PLSCheckBox::PLSCheckBox(const QString &text, QWidget *parent) : QWidget(parent)
{
	pls_add_css(this, {"QCheckBox"});
	m_layout = pls_new<QHBoxLayout>(this);
	m_layout->setContentsMargins(ICON_SIZE.width() + m_spac, 0, 0, 0);
	m_layout->setSpacing(0);

	m_text = pls_new<QLabel>(text, this);
	m_text->setObjectName(QStringLiteral("plsCheckBox_text"));
	m_text->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_layout->addWidget(m_text);

	pls_button_uistep_custom(this, [this]() -> QVariant { return isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"); });
}

PLSCheckBox::PLSCheckBox(const QPixmap &pixmap, const QString &text, bool textFirst, const QString &tooltip, QWidget *parent) : QWidget(parent)
{
	pls_add_css(this, {"QCheckBox"});
	m_layout = pls_new<QHBoxLayout>(this);
	m_layout->setContentsMargins(ICON_SIZE.width() + m_spac, 0, 0, 0);
	m_layout->setSpacing(5);

	m_text = pls_new<QLabel>(text, this);
	m_text->setObjectName(QStringLiteral("plsCheckBox_text"));
	m_text->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto image = pls_new<QLabel>();
	image->setObjectName("showImageLable");
	image->setScaledContents(true);
	image->setPixmap(pixmap);
	if (!tooltip.isEmpty())
		image->setToolTip(tooltip);
	if (textFirst) {
		m_layout->addWidget(m_text);
		m_layout->addWidget(image);
	} else {
		m_layout->addWidget(image);
		m_layout->addWidget(m_text);
	}
}

QSize PLSCheckBox::iconSize() const
{
	return ICON_SIZE;
}

bool PLSCheckBox::isChecked() const
{
	return m_checked;
}

void PLSCheckBox::setText(const QString &text)
{
	m_text->setText(text);
}
QString PLSCheckBox::text() const
{
	return m_text->text();
}

Qt::CheckState PLSCheckBox::checkState() const
{
	return m_checked ? Qt::Checked : Qt::Unchecked;
}
void PLSCheckBox::setCheckState(Qt::CheckState state)
{
	setChecked(state == Qt::Checked);
}

int PLSCheckBox::getSpac() const
{
	return m_spac;
}

void PLSCheckBox::setSpac(int spac)
{
	m_spac = spac;
	m_layout->setContentsMargins(ICON_SIZE.width() + m_spac, 0, 0, 0);
}

void PLSCheckBox::setIconSize(const QSize &size) {}
void PLSCheckBox::animateClick() {}
void PLSCheckBox::click()
{
	m_pressed = false;
	released();
	clicked(m_checked);
}
void PLSCheckBox::toggle()
{
	setChecked(!m_checked);
}
void PLSCheckBox::setChecked(bool checked)
{
	if (m_checked != checked) {
		m_checked = checked;
		repaint();
		toggled(m_checked);
		stateChanged(m_checked ? Qt::Checked : Qt::Unchecked);
	}
}

void PLSCheckBox::setState(const char *name, bool &state, bool value)
{
	if (state != value) {
		state = value;
		repaint();
	}
}

const QPixmap &PLSCheckBox::getIcon() const
{
	auto app = PLSUiApp::instance();
	if (!app)
		return m_defIcon;

	if (m_checked) {
		if (!isEnabled()) {
			return app->m_checkBoxIcons[PLSUiApp::CheckedDisabled];
		} else if (m_hovered || m_pressed) {
			return app->m_checkBoxIcons[m_pressed ? PLSUiApp::CheckedPressed : PLSUiApp::CheckedHover];
		} else {
			return app->m_checkBoxIcons[PLSUiApp::CheckedNormal];
		}
	} else {
		if (!isEnabled()) {
			return app->m_checkBoxIcons[PLSUiApp::UncheckedDisabled];
		} else if (m_hovered || m_pressed) {
			return app->m_checkBoxIcons[m_pressed ? PLSUiApp::UncheckedPressed : PLSUiApp::UncheckedHover];
		} else {
			return app->m_checkBoxIcons[PLSUiApp::UncheckedNormal];
		}
	}
}

bool PLSCheckBox::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::EnabledChange:
		update();
		break;
	case QEvent::LayoutDirectionChange:
		m_layout->setContentsMargins(0, 0, ICON_SIZE.width() + m_spac, 0);
		update();
		break;
	case QEvent::Enter:
		if (isEnabled())
			setState("hovered", m_hovered, true);
		break;
	case QEvent::Leave:
		if (isEnabled())
			setState("hovered", m_hovered, false);
		break;
	case QEvent::MouseButtonPress:
		if (isEnabled()) {
			setState("pressed", m_pressed, true);
			pressed();
		}
		break;
	case QEvent::MouseButtonRelease:
		if (isEnabled()) {
			setState("pressed", m_pressed, false);
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				toggle();
				click();
			}
		}
		break;
	case QEvent::MouseMove:
		if (isEnabled())
			setState("hovered", m_hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
		break;
	default:
		break;
	}
	return QWidget::event(event);
}

void PLSCheckBox::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	if (const auto &icon = getIcon(); !icon.isNull()) {
		QPixmap pix = icon.scaled(ICON_SIZE * devicePixelRatio(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		pix.setDevicePixelRatio(devicePixelRatio());

		QRect rc = rect();
		auto direction = layoutDirection();
		if (direction == Qt::LeftToRight || direction == Qt::LayoutDirectionAuto)
			painter.drawPixmap(QPointF(0, (rc.height() - ICON_SIZE.height()) / 2.0), pix);
		else
			painter.drawPixmap(QPointF(rc.right() - ICON_SIZE.width(), (rc.height() - ICON_SIZE.height()) / 2.0), pix);
	}
}

PLSElideCheckBox::PLSElideCheckBox(QWidget *parent) : PLSCheckBox(parent) {}

PLSElideCheckBox::PLSElideCheckBox(const QString &text, QWidget *parent) : PLSCheckBox(text, parent), originText(text) {}

void PLSElideCheckBox::setText(const QString &text)
{
	originText = text;
	updateText();
}

QString PLSElideCheckBox::text() const
{
	return originText;
}

QSize PLSElideCheckBox::sizeHint() const
{
	return QSize(0, PLSCheckBox::sizeHint().height());
}

QSize PLSElideCheckBox::minimumSizeHint() const
{
	return QSize(0, PLSCheckBox::minimumSizeHint().height());
}

void PLSElideCheckBox::updateText()
{
	if (originText.isEmpty()) {
		PLSCheckBox::setText(originText);
		return;
	}

	QString text = this->fontMetrics().elidedText(originText, Qt::ElideRight, width() - iconSize().width() - SPACING);
	PLSCheckBox::setText(text);
}

void PLSElideCheckBox::resizeEvent(QResizeEvent *event)
{
	PLSCheckBox::resizeEvent(event);
	updateText();
}
