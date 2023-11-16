#pragma once

#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>
#include <QCheckBox>

bool isButtonStateNeedUpdate();

template<typename BaseT> class PressRestoreButton : public BaseT {

private:
	PressRestoreButton(const PressRestoreButton &) = delete;
	PressRestoreButton &operator=(const PressRestoreButton &) = delete;

public:
	explicit PressRestoreButton(QWidget *parent = nullptr) : BaseT(parent)
	{
		BaseT::connect(&m_timer, &QTimer::timeout, this, [this] {
			if (!this->isDown()) {
				m_timer.stop();
			} else if (isButtonStateNeedUpdate()) {
				m_timer.stop();
				this->setDown(false);
				emit this->clicked();
			}
		});
	};
	~PressRestoreButton() override
	{
		if (m_timer.isActive()) {
			m_timer.stop();
		}
	};

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		BaseT::mousePressEvent(event);
		m_timer.stop();
		m_timer.start(100);
	};
	void mouseReleaseEvent(QMouseEvent *event) override
	{
		BaseT::mouseReleaseEvent(event);
		m_timer.stop();
	};

private:
	QTimer m_timer;
};

using PLSRestoreCheckBox = PressRestoreButton<QCheckBox>;
using PLSPressRestoreButton = PressRestoreButton<QPushButton>;
