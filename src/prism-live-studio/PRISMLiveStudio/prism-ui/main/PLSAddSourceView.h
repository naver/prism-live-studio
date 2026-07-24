#ifndef PLSADDSOURCEVIEW_H
#define PLSADDSOURCEVIEW_H

#include <QWidget>
#include "PLSDialogView.h"
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qmovie.h>
#include <QPointer>
#include "PLSAddSourceGuideView.h"
#include "PLSSearchLineEdit.h"

#define SOURCSHORTEDESCCONTENT QStringLiteral("sourceMenu.%1.item.short.desc")
#define GUID_EVIEW_RESULT_VALUE 100

namespace Ui {
class PLSAddSourceView;
class PLSAddSourceItem;
}
class QScrollArea;
class QGridLayout;
class QGroupBox;
class PLSAnimationLabel;
class PLSAddSourceItem : public QPushButton {
	Q_OBJECT

public:
	explicit PLSAddSourceItem(const QString &id, const QString &displayName, bool isThirdPlugin = false, bool isBeta = false, QWidget *parent = nullptr);
	~PLSAddSourceItem() override;
	QString itemId() const;
	QString itemIconKey() const;
	QString itemDisplayName() const;
	QString itemShortDesc() const;
	void setScrollArea(QScrollArea *scrollArea);
	QScrollArea *getScrollArea();
	void calculateLabelWidth(bool isNew, bool isChecked);
	void setHighlightText(const QString &searchText);

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void statusChanged(bool isChecked);

private:
	Ui::PLSAddSourceItem *ui;
	QString m_id;
	QString m_text;
	QString m_iconKey;
	QString m_shortDesc;
	QScrollArea *m_scrollArea = nullptr;
	QPointer<QLabel> m_newLabel;
	bool m_isNew = false;
	bool m_isBeta = false;
	QPointer<QLabel> m_betaLabel;
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
	QStringList getNeedAddSourceList();

	static QString getSourceShortDesc(const char *sourceId, const char *iconKey);
	static QString highlightText(const QString &text, const QString &searchText);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void initSourceItems();
	void initSourceLayout(int startIndex = 0, int groupCount = 3);
	void setSourceDesc(QAbstractButton *button);
	void setLangShort();
	void calculateDescHeight(QLabel *label, bool isHtml = false);
	void filterSourceItems(const QString &searchText);
	void updateEmptyState(bool isEmpty);
	void stopDescVideo();
	void playDescVideo(const QString &resourcePath, const QSize &size);

private slots:
	void sourceItemChanged(QAbstractButton *button);
	void okHandler();
	void openSourceGuideDialog();
	void onSearchTextChanged(const QString &text);

private:
	Ui::PLSAddSourceView *ui;
	QButtonGroup m_buttonGroup;
	QMovie m_movie;
	QStringList m_langShortList = {"en", "ko"};
	QString m_langShort = "en";
	QVector<QPair<std::string, QVector<PLSAddSourceItem *>>> m_sourceItems;
	QVector<QGridLayout *> m_layout;
	QVector<QGroupBox *> m_groupBoxes;
	QStringList m_needAddSourceList;
	int m_displayCount = -1;
	PLSSearchLineEdit *m_searchLineEdit = nullptr;
	QWidget *m_emptyStateWidget = nullptr;
	QString m_lastTrimmedSearch;
	QPointer<PLSAnimationLabel> m_descVideoLabel;
};

#endif // PLSADDSOURCEVIEW_H
