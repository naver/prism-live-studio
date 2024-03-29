#ifndef PLSLOADINGCOMBOXMENU_H
#define PLSLOADINGCOMBOXMENU_H

#include <QMenu>
#include <QListWidget>
#include <QPointer>

const int MENU_DONT_SELECTED_INDEX = -100;

class PLSLoadingMenuItem;

namespace Ui {
class PLSLoadingComboxMenu;
}

enum class PLSLoadingComboxItemType {
	Fa_NormalTitle,
	Fa_Loading,
	Fa_Guide,
};

struct PLSLoadingComboxItemData {
	int showIndex = 0;
	QString title;
	PLSLoadingComboxItemType type = PLSLoadingComboxItemType::Fa_NormalTitle;
};
Q_DECLARE_METATYPE(PLSLoadingComboxItemData)

class PLSLoadingComboxMenu : public QMenu {
	Q_OBJECT

public:
	explicit PLSLoadingComboxMenu(QWidget *parent = nullptr);
	~PLSLoadingComboxMenu() override;
	void showLoading(QWidget *parent);
	void showTitleListView(QWidget *parent, int showIndex, QStringList list);
	void showGuideTipView(QWidget *parent, QString guide);
	void setSelectedTitleItemIndex(int showIndex) const;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void setupCornerRadius();

signals:
	void itemDidSelect(QListWidgetItem *item);

private:
	void showTitleItemsView(QWidget *parent, int showIndex, PLSLoadingComboxItemType type, QStringList list);

	Ui::PLSLoadingComboxMenu *ui;
	QListWidget *m_listWidget;
	QList<PLSLoadingMenuItem *> m_items;
	QPointer<QWidget> m_parent;
};

#endif // PLSLOADINGCOMBOXMENU_H
