#ifndef PLSSEARCHLINEEDIT_H
#define PLSSEARCHLINEEDIT_H

#include <QLineEdit>
#include <QPushButton>
#include "libui.h"

class LIBUI_API PLSSearchLineEdit : public QLineEdit {
	Q_OBJECT
public:
	explicit PLSSearchLineEdit(QWidget *parent = nullptr);
	~PLSSearchLineEdit() override = default;

	void SetDeleteBtnVisible(bool visible);
signals:
	void SearchTrigger(const QString &key);
	void SearchMenuRequested(bool show);
    void SearchIconClicked(const QString &key);

protected:
	void focusInEvent(QFocusEvent *e) override;
	void focusOutEvent(QFocusEvent *e) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
    void updatePlaceHolderColor();
    
	QToolButton *toolBtnSearch;
	QPushButton *deleteBtn;
};

#endif //PLSSEARCHLINEEDIT_H
