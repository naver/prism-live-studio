#ifndef PLSSCHEDULEMENU_H
#define PLSSCHEDULEMENU_H
#include <QMenu>
#include <QWidget>

#include <vector>
#include <QListWidget>
#include "PLSScheduleMenuItem.h"

using namespace std;

class PLSScheduleMenu : public QMenu {
	Q_OBJECT

public:
	explicit PLSScheduleMenu(QWidget *parent = nullptr);
	~PLSScheduleMenu();
	void setupDatas(const vector<ComplexItemData> &datas, int btnWidth);

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event);

private:
	QListWidget *m_listWidget;
	void itemDidSelect(QListWidgetItem *item);

signals:
	void scheduleItemClicked(const QString data);
};

#endif // PLSSCHEDULEMENU_H
