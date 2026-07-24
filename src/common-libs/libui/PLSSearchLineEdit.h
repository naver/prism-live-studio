#ifndef PLSSEARCHLINEEDIT_H
#define PLSSEARCHLINEEDIT_H

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include "libui.h"

class QToolButton;

class LIBUI_API PLSSearchLineEdit : public QLineEdit {
	Q_OBJECT
public:
	enum SearchButtonPosition {
		SearchButtonLeft,
		SearchButtonRight
	};
	Q_ENUM(SearchButtonPosition)

	explicit PLSSearchLineEdit(QWidget *parent = nullptr);
	~PLSSearchLineEdit() override = default;

	void SetDeleteBtnVisible(bool visible);
	void setSearchButtonPosition(SearchButtonPosition position);

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
	void updateLayout();

	QToolButton *toolBtnSearch;
	QPushButton *deleteBtn;
	QHBoxLayout *searchLayout;
	SearchButtonPosition m_searchButtonPosition = SearchButtonRight;
};

#endif //PLSSEARCHLINEEDIT_H
