#include "PLSRadioButton.h"
#include "PLSUIApp.h"

#include <qevent.h>
#include <qlabel.h>
#include <qboxlayout.h>
#include <qpainter.h>

#include <libui.h>
#include <libutils-api.h>

constexpr QSize ICON_SIZE{18, 18};
constexpr int SPACING = 6;
constexpr int FACTOR = 4;

PLSRadioButtonGroup::PLSRadioButtonGroup(QObject *parent) : QObject(parent) {}

PLSRadioButton *PLSRadioButtonGroup::button(int id) const
{
	for (auto button : m_btns)
		if (button->id() == id)
			return button;
	return nullptr;
}
QList<PLSRadioButton *> PLSRadioButtonGroup::buttons() const
{
	return m_btns;
}

PLSRadioButton *PLSRadioButtonGroup::checkedButton() const
{
	for (auto button : m_btns)
		if (button->isChecked())
			return button;
	return nullptr;
}
int PLSRadioButtonGroup::checkedId() const
{
	for (auto button : m_btns)
		if (button->isChecked())
			return button->id();
	return -1;
}

int PLSRadioButtonGroup::id(PLSRadioButton *button) const
{
	if (m_btns.contains(button))
		return button->id();
	return -1;
}
void PLSRadioButtonGroup::setId(PLSRadioButton *button, int id)
{
	if (m_btns.contains(button))
		button->setId(id);
}

void PLSRadioButtonGroup::addButton(PLSRadioButton *button, int id)
{
	if (m_btns.contains(button))
		return;

	m_btns.append(button);
	button->setId(id < 0 ? m_btns.count() : id);

	connect(button, &PLSRadioButton::clicked, this, [button, this]() {
		buttonClicked(button);
		idClicked(button->id());
	});
	connect(button, &PLSRadioButton::pressed, this, [button, this]() {
		buttonPressed(button);
		idPressed(button->id());
	});
	connect(button, &PLSRadioButton::released, this, [button, this]() {
		buttonReleased(button);
		idReleased(button->id());
	});
	connect(button, &PLSRadioButton::toggled, this, [button, this](bool checked) {
		uncheckAllBut(button);
		buttonToggled(button, checked);
		idToggled(button->id(), checked);
	});
	connect(button, &QObject::destroyed, this, [button, this]() { //
		m_btns.removeOne(button);
	});
}
void PLSRadioButtonGroup::removeButton(PLSRadioButton *button)
{
	button->disconnect(this);
	m_btns.removeOne(button);
	if (m_btns.isEmpty()) {
		pls_delete(this);
	}
}

void PLSRadioButtonGroup::uncheckAllBut(PLSRadioButton *button)
{
	for (auto btn : m_btns) {
		if (btn != button) {
			QSignalBlocker blocker(btn);
			btn->setChecked(false);
		}
	}
}

PLSRadioButton::PLSRadioButton(QWidget *parent) : PLSRadioButton(QString(), parent) {}
PLSRadioButton::PLSRadioButton(const QString &text, QWidget *parent) : QWidget(parent)
{
	initGroup(parent);

	m_layout = pls_new<QHBoxLayout>(this);
	m_layout->setContentsMargins(ICON_SIZE.width() + SPACING, 0, 0, 0);
	m_layout->setSpacing(0);

	m_text = pls_new<QLabel>(text, this);
	m_text->setObjectName(QStringLiteral("plsRadioButton_text"));
	m_text->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_layout->addWidget(m_text);
}

QSize PLSRadioButton::iconSize() const
{
	return ICON_SIZE;
}

bool PLSRadioButton::isChecked() const
{
	return m_checked;
}

void PLSRadioButton::setText(const QString &text)
{
	m_text->setText(text);
}
QString PLSRadioButton::text() const
{
	return m_text->text();
}

int PLSRadioButton::id() const
{
	return m_id;
}
void PLSRadioButton::setId(int id)
{
	m_id = id;
}

PLSRadioButtonGroup *PLSRadioButton::group() const
{
	return m_group;
}

void PLSRadioButton::setIconSize(const QSize &size) {}
void PLSRadioButton::animateClick() {}
void PLSRadioButton::click()
{
	m_pressed = false;
	released();
	clicked(m_checked);
}
void PLSRadioButton::toggle()
{
	setChecked(true);
}
void PLSRadioButton::setChecked(bool checked)
{
	if (m_checked != checked) {
		m_checked = checked;
		repaint();
		toggled(m_checked);
	}
}

void PLSRadioButton::initGroup(QWidget *parent)
{
	if (!parent)
		return;

	for (auto c : parent->children()) {
		if (c == this || !c->isWidgetType())
			continue;
		else if (auto w = dynamic_cast<PLSRadioButton *>(c); w) {
			m_group = w->group();
			m_group->addButton(this);
			return;
		}
	}

	if (m_group) {
		m_group->removeButton(this);
	}

	m_group = pls_new<PLSRadioButtonGroup>(parent);
	m_group->addButton(this);
}

void PLSRadioButton::setState(const char *name, bool &state, bool value)
{
	if (state != value) {
		state = value;
		repaint();
	}
}

const QPixmap &PLSRadioButton::getIcon() const
{
	auto app = PLSUiApp::instance();
	if (!app)
		return m_defIcon;

	if (m_checked) {
		if (!isEnabled()) {
			return app->m_radioButtonIcons[PLSUiApp::CheckedDisabled];
		} else if (m_hovered || m_pressed) {
			return app->m_radioButtonIcons[m_pressed ? PLSUiApp::CheckedPressed : PLSUiApp::CheckedHover];
		} else {
			return app->m_radioButtonIcons[PLSUiApp::CheckedNormal];
		}
	} else {
		if (!isEnabled()) {
			return app->m_radioButtonIcons[PLSUiApp::UncheckedDisabled];
		} else if (m_hovered || m_pressed) {
			return app->m_radioButtonIcons[m_pressed ? PLSUiApp::UncheckedPressed : PLSUiApp::UncheckedHover];
		} else {
			return app->m_radioButtonIcons[PLSUiApp::UncheckedNormal];
		}
	}
}

bool PLSRadioButton::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::ParentChange:
		initGroup(parentWidget());
		break;
	case QEvent::EnabledChange:
		update();
		break;
	case QEvent::LayoutDirectionChange:
		m_layout->setContentsMargins(0, 0, ICON_SIZE.width() + SPACING, 0);
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

void PLSRadioButton::paintEvent(QPaintEvent *)
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

PLSElideRadioButton::PLSElideRadioButton(QWidget *parent) : PLSRadioButton(parent) {}

PLSElideRadioButton::PLSElideRadioButton(const QString &text, QWidget *parent) : PLSRadioButton(text, parent), originText(text) {}

void PLSElideRadioButton::setText(const QString &text)
{
	originText = text;
	updateText();
}

QString PLSElideRadioButton::text() const
{
	return originText;
}

QSize PLSElideRadioButton::sizeHint() const
{
	return QSize(0, PLSRadioButton::sizeHint().height());
}

QSize PLSElideRadioButton::minimumSizeHint() const
{
	return QSize(0, PLSRadioButton::minimumSizeHint().height());
}

void PLSElideRadioButton::updateText()
{
	if (originText.isEmpty()) {
		PLSRadioButton::setText(originText);
		return;
	}

	QString text = this->fontMetrics().elidedText(originText, Qt::ElideRight, width() - iconSize().width() - SPACING);
	PLSRadioButton::setText(text);
}

void PLSElideRadioButton::resizeEvent(QResizeEvent *event)
{
	PLSRadioButton::resizeEvent(event);
	updateText();
}
