#ifndef PLSADDSOURCEVIEW_H
#define PLSADDSOURCEVIEW_H

#include <QWidget>
#include "PLSDialogView.h"
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
	explicit PLSAddSourceItem(const QString &id, const QString &displayName, bool isThirdPlugin = false, QWidget *parent = nullptr);
	~PLSAddSourceItem() override;
	QString itemId() const;
	QString itemIconKey() const;
	QString itemDisplayName() const;
	void setScrollArea(QScrollArea *scrollArea);
	QScrollArea *getScrollArea();
	void calculateLabelWidth(bool isNew, bool isChecked);

private slots:
	void statusChanged(bool isChecked);

private:
	Ui::PLSAddSourceItem *ui;
	QString m_id;
	QString m_text;
	QString m_iconKey;
	QScrollArea *m_scrollArea = nullptr;
	QPointer<QLabel> m_newLabel;
};

class PLSAddSourceView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSAddSourceView(QWidget *parent = nullptr);
	~PLSAddSourceView() override;
	static PLSAddSourceView *instance()
	{
		static PLSAddSourceView view;
		return &view;
	}
	QString selectSourceId() const;

private:
	void initSourceItems();
	void setSourceDesc(QAbstractButton *button);
	void setLangShort();
	void calculateDescHeight(QLabel *label);

private slots:
	void sourceItemChanged(QAbstractButton *button);
	void okHandler();
	void openSourceLink();

private:
	Ui::PLSAddSourceView *ui;
	QButtonGroup m_buttonGroup;
	QMovie m_movie;
	QStringList m_langShortList = {"en", "ko"};
	QString m_langShort = "en";
	QString m_openLink;
};

#endif // PLSADDSOURCEVIEW_H
