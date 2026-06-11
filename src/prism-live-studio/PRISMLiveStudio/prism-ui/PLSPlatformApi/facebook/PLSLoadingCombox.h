#ifndef PLSLOADINGCOMBOX_H
#define PLSLOADINGCOMBOX_H

#include <QPushButton>
#include "PLSLoadingComboxMenu.h"

namespace Ui {
class PLSLoadingCombox;
}

class PLSLoadingCombox : public QPushButton {
	Q_OBJECT

public:
	explicit PLSLoadingCombox(QWidget *parent = nullptr);
	~PLSLoadingCombox() override;
	void showLoadingView();
	void showTitlesView(QStringList list);
	void showTitleIdsView(QStringList list, QStringList titleIds);
	void updateTitlesView(QStringList titles);
	void updateTitleIdsView(QStringList titles, QStringList titleIds);
	void refreshGuideView(QString guide);
	void hidenMenuView();
	void setComboBoxTitleData(const QString title, const QString id = nullptr);
	QString &getComboBoxTitle();
	QString &getComboBoxId();
	bool eventFilter(QObject *watched, QEvent *event) override;

signals:
	void clickItemIndex(int index);

private:
	void showComboListView(bool show);
	void itemDidSelect(const QListWidgetItem *item);
	void showMenuView();

	Ui::PLSLoadingCombox *ui;
	PLSLoadingComboxMenu *m_menu;
	QString m_title;
	QString m_id;
};

#endif // PLSLOADINGCOMBOX_H
