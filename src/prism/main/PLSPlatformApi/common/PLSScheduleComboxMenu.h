#pragma once
#include <qmetatype.h>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QTimer>
#include <QWidget>
#include <vector>

const extern int s_itemHeight_30;
const extern int s_itemHeight_40;
const extern int s_itemHeight_53;
const extern int s_itemHeight_70;

enum class PLSScheComboxItemType {
	Ty_NormalLive,
	Ty_Loading,
	Ty_Header,
	Ty_Placehoder,
	Ty_Schedule,
};

struct PLSScheComboxItemData {
	QString _id;
	QString title;
	long timeStamp = 0;
	PLSScheComboxItemType type = PLSScheComboxItemType::Ty_NormalLive;
	QString time;
	QString imgUrl;

	//the blow data is use for vlive
	bool needShowTimeLeftTimer = false;
	bool isNewLive = true;
	long endTimeStamp = 0;
	int itemHeight = s_itemHeight_70;
	bool isShowRightIcon = false;
	bool isSelect = false;
	QString platformName;
	bool isExpired = false;
};

Q_DECLARE_METATYPE(PLSScheComboxItemData)

using namespace std;
class PLSScheduleComboxItem;

class PLSScheduleComboxMenu : public QMenu {
	Q_OBJECT

public:
	explicit PLSScheduleComboxMenu(QWidget *parent = nullptr);
	~PLSScheduleComboxMenu();
	void setupDatas(const vector<PLSScheComboxItemData> &datas, int btnWidth);
	static QString getDetailTime(const PLSScheComboxItemData &data, bool &willDelete);

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event);
	virtual void hideEvent(QHideEvent *event);

private:
	QListWidget *m_listWidget;
	QTimer *m_pLeftTimer;

	vector<PLSScheduleComboxItem *> m_vecItems;
	void addAItem(const QString &showStr, const PLSScheComboxItemData &itemData, int itemSize);
	void setupTimer();
	void itemDidSelect(QListWidgetItem *item);
	static QString formateTheLeftTime(long leftTimeStamp);

private slots:
	void updateDetailLabel();

signals:
	void scheduleItemClicked(const QString data);
	void scheduleItemExpired(vector<QString> ids);
};

namespace Ui {
class PLSScheduleComboxItem;
}

class PLSScheduleComboxItem : public QWidget {
	Q_OBJECT
public:
	explicit PLSScheduleComboxItem(const PLSScheComboxItemData data, double dpi, QWidget *parent = nullptr);
	~PLSScheduleComboxItem();

	void setDetailLabelStr(const QString &str);
	const PLSScheComboxItemData &getData() { return _data; };

private:
	Ui::PLSScheduleComboxItem *ui;

	const PLSScheComboxItemData _data;
	QTimer *m_pTimer;
	double m_Dpi;

	void setupTimer();
	QString PLSScheduleComboxItem::GetNameElideString();

	void downloadThumImage(QLabel *reciver, const QString &url);

private slots:
	void updateProgress();

protected:
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
	virtual void resizeEvent(QResizeEvent *event) override;
};
