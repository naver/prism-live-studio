#ifndef PLSCHATFONTZOOMFRAME_H
#define PLSCHATFONTZOOMFRAME_H

#include <QFrame>

namespace Ui {
class PLSChatFontZoomFrame;
}

class PLSChatFontZoomFrame : public QFrame {
	Q_OBJECT

public:
	explicit PLSChatFontZoomFrame(QWidget *parent = nullptr, QWidget *ignoreWidget = nullptr);
	~PLSChatFontZoomFrame() final;

	bool eventFilter(QObject *i_Object, QEvent *i_Event) final;

private:
	Ui::PLSChatFontZoomFrame *ui;
	QWidget *m_ignoreWidget;

	void fontChangeBtnClick(bool isPlus);

	void updateUIWithScale(int scale);
};

#endif // PLSCHATFONTZOOMFRAME_H
