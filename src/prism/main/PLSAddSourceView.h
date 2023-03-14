#ifndef PLSADDSOURCEVIEW_H
#define PLSADDSOURCEVIEW_H

#include <QWidget>
#include "dialog-view.hpp"
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qmovie.h>

namespace Ui {
class PLSAddSourceView;
class PLSAddSourceItem;
}
class QScrollArea;

class PLSAddSourceItem : public QPushButton {
	Q_OBJECT

public:
	explicit PLSAddSourceItem(const QString &id, const QString &displayName, PLSDpiHelper dpiHelper, bool isThirdPlugin = false, QWidget *parent = nullptr);
	~PLSAddSourceItem();
	void updateItemStatus(bool isSelected);
	QString itemId();
	QString itemIconKey();
	QString itemDisplayName();
	void setScrollArea(QScrollArea *scrollArea);
	QScrollArea *getScrollArea();

private slots:
	void statusChanged(bool isChecked);

private:
	Ui::PLSAddSourceItem *ui;
	QString m_id;
	QString m_text;
	QString m_iconKey;
	QScrollArea *m_scrollArea = nullptr;
};

class PLSAddSourceView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSAddSourceView(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSAddSourceView();
	static PLSAddSourceView *instance()
	{
		static PLSAddSourceView view;
		return &view;
	}
	QString selectSourceId();

private:
	void initSourceItems(const PLSDpiHelper &dpiHelper);
	void setSourceDesc(QAbstractButton *button);
	void setLangShort();
	QString titleLineFeed(const QString &title);

private slots:
	void sourceItemChanged(QAbstractButton *button);
	void okHandler();

private:
	Ui::PLSAddSourceView *ui;
	QButtonGroup m_buttonGroup;
	QMovie m_movie;
	QStringList m_langShortList;
	QString m_langShort = "en";
};

#endif // PLSADDSOURCEVIEW_H
