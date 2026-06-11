#pragma once

#include <QFrame>

class GuideButton : public QFrame {
	bool hovered = false;
	bool pressed = false;
	std::function<void()> clicked;

public:
	GuideButton(const QString &buttonText, bool fromLeftToRight, QWidget *parent, std::function<void()> clicked_);

	~GuideButton() override = default;
	void setState(const char *name, bool &state, bool value);

protected:
	bool event(QEvent *event) override;
};
