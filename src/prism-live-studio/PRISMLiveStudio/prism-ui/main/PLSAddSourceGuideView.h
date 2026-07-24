#ifndef PLSADDSOURCEGUIDEVIEW_H
#define PLSADDSOURCEGUIDEVIEW_H

#include "PLSDialogView.h"
#include <PLSCheckBox.h>
#include <QButtonGroup>
#include <QStringList>

namespace Ui {
class PLSAddSourceGuideView;
}

struct SourceGuideAttr {
	const char *sourceId{nullptr};
	QString displayName;
	bool isChecked = true;
};

class PLSAddSourceGuideView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSAddSourceGuideView(QWidget *parent = nullptr);
	void initUi();
	~PLSAddSourceGuideView();
	static QVector<QPair<QPair<const char *, bool>, QVector<SourceGuideAttr>>> *initSourceTabList();
	const QStringList &selectSourceList();
	const int currentTabIndex() { return m_currentTabIndex; }

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void updateSourceListLayout(int id);
	void on_sourceAddButton_clicked();
	void sourceListStateChanged(int status);

private:
	Ui::PLSAddSourceGuideView *ui;
	QVector<PLSCheckBox *> m_sourceChecBox;
	QButtonGroup *m_sourceTabButtonGroup = nullptr;
	const QVector<QPair<QPair<const char *, bool>, QVector<SourceGuideAttr>>> *m_sourceTabList = nullptr;
	QVector<QString> m_iconPath;
	QStringList m_selectSourceList;
	int m_currentTabIndex = -1;
};

#endif // PLSADDSOURCEGUIDEVIEW_H
