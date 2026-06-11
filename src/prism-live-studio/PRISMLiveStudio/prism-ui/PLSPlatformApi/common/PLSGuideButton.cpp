#include "PLSGuideButton.h"
#include <QLabel>
#include <frontend-api.h>
#include <QHBoxLayout>
#include "libui.h"
#include <QMouseEvent>

GuideButton::GuideButton(const QString &buttonText, bool fromLeftToRight, QWidget *parent, std::function<void()> clicked_) : QFrame(parent), clicked(std::move(clicked_))
{
	setObjectName("guideButton");
	setProperty("lang", pls_get_current_language());
	setMouseTracking(true);

	auto icon = pls_new<QLabel>(this);
	icon->setObjectName("guideButtonIcon");
	icon->setMouseTracking(true);
	icon->setAlignment(Qt::AlignCenter);

	auto text = pls_new<QLabel>(this);
	text->setObjectName("guideButtonText");
	text->setMouseTracking(true);
	text->setText(buttonText);
	text->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);

	auto layout = pls_new<QHBoxLayout>(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(5);
	if (fromLeftToRight) {
		layout->addWidget(icon);
		layout->addWidget(text);
	} else {
		layout->addWidget(text);
		layout->addWidget(icon);
	}
}

void GuideButton::setState(const char *name, bool &state, bool value)
{
	if (state != value) {
		pls_flush_style_recursive(this, name, state = value);
	}
}

bool GuideButton::event(QEvent *event)
{
	if (event->type() == QEvent::Enter) {
		setState("hovered", hovered, true);
	} else if (event->type() == QEvent::Leave) {
		setState("hovered", hovered, false);
	} else if (event->type() == QEvent::MouseButtonPress) {
		setState("pressed", pressed, true);
	} else if (event->type() == QEvent::MouseButtonRelease) {
		setState("pressed", pressed, false);
		if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
			clicked();
		}
	} else if (event->type() == QEvent::MouseMove) {
		setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
	}
	return QFrame::event(event);
}
