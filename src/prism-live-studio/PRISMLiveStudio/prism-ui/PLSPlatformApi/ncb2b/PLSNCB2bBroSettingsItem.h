#ifndef PLSNCB2BBROSETTINGSITEM_H
#define PLSNCB2BBROSETTINGSITEM_H

#include <QFrame>
#include <QObject>

namespace Ui {
class PLSNCB2bBroSettingsItem;
}

struct PLSNCB2bBrowserSettingData {
	QString title;
	QString url;
	bool selected{true};
};

class PLSNCB2bBroSettingsManager : public QObject {
	Q_OBJECT

public:
	static PLSNCB2bBroSettingsManager *instance();
	QList<PLSNCB2bBrowserSettingData> getDatas();
	QList<PLSNCB2bBrowserSettingData> getDatas(bool selected);
	QList<QString> getTitles(bool selected);
	void initDatas(const QList<PLSNCB2bBrowserSettingData> &datas);
	void setSelected(const PLSNCB2bBrowserSettingData &data, bool selected);
	void setSelected(const QList<PLSNCB2bBrowserSettingData> &datas);
	bool getSelected(const PLSNCB2bBrowserSettingData &data);

private:
	QList<PLSNCB2bBrowserSettingData> datas;
};

class PLSNCB2bBroSettingsItem : public QFrame {
	Q_OBJECT

public:
	explicit PLSNCB2bBroSettingsItem(const PLSNCB2bBrowserSettingData &data, QWidget *parent = nullptr);
	~PLSNCB2bBroSettingsItem() override;
	const PLSNCB2bBrowserSettingData &getData();
	void setChecked(bool checked);

private:
	void initUI();
signals:
	void itemSelected(bool select);

private:
	Ui::PLSNCB2bBroSettingsItem *ui;
	PLSNCB2bBrowserSettingData data;
};

#endif // PLSNCB2BBROSETTINGSITEM_H
