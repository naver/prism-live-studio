#ifndef PLSCOMMONSCROLLBAR_H
#define PLSCOMMONSCROLLBAR_H

#include <QObject>
#include <qscrollbar.h>
#include "libui.h"

class LIBUI_API PLSCommonScrollBar : public QScrollBar {
	Q_OBJECT
public:
	explicit PLSCommonScrollBar(QWidget *parent = nullptr);
	~PLSCommonScrollBar() final = default;

	// QWidget interface
protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
signals:
	void isShowScrollBar(bool isShow);
};

#endif // PLSCOMMONSCROLLBAR_H
