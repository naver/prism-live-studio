#ifndef PLSSCROLLINGLABEL_H
#define PLSSCROLLINGLABEL_H

#include <QLabel>
#include "libui.h"

class LIBUI_API PLSScrollingLabel : public QLabel {
	Q_OBJECT
public:
	explicit PLSScrollingLabel(QWidget *parent = nullptr);
	~PLSScrollingLabel() override;
	PLSScrollingLabel(const PLSScrollingLabel &) = delete;
	PLSScrollingLabel &operator=(const PLSScrollingLabel &) = delete;
	void SetText(const QString &text);

protected:
	void paintEvent(QPaintEvent *event) override;
	void timerEvent(QTimerEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

protected slots:
	void handleScorllingText();

private:
	void StartTimer(const int interval = 100);
	void StopTimer();
	void UpateRollingStatus();

	int timerId{-1};
	int left{0};
	QString text{};
};

#endif // PLSSCROLLINGLABEL_H
