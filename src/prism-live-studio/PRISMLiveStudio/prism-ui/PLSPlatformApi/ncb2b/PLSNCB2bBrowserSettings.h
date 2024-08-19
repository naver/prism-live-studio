#ifndef PLSNCB2BBROWSERSETTINGS_H
#define PLSNCB2BBROWSERSETTINGS_H

#include "PLSSideBarDialogView.h"
#include "PLSNCB2bBroSettingsItem.h"

namespace Ui {
class PLSNCB2bBrowserSettings;
}

class PLSNCB2bBrowserSettings : public PLSSideBarDialogView {
	Q_OBJECT

public:
	explicit PLSNCB2bBrowserSettings(DialogInfo info, QWidget *parent = nullptr);
	~PLSNCB2bBrowserSettings() override;

	void refreshUI();

protected:
	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void onOkButtonClicked();
	void onRefreshButtonClicked();

private:
	void createItems(const QList<PLSNCB2bBrowserSettingData> &datas);
	void createItem(const PLSNCB2bBrowserSettingData &data);
	void updateUI(const QList<PLSNCB2bBrowserSettingData> &datas);
	void updateLogo();
	void removeAll();
	void updateDocks();
	void updateDock(const PLSNCB2bBrowserSettingData &data, int index, QByteArray geometry);
	bool getDockChecked(const QString &title);
	void updateSelected();
	void updateChecked();
	QList<PLSNCB2bBrowserSettingData> parseSupportUrls(const QJsonObject &obj);
	QString getDisplayTitle(const QString &title);

private:
	Ui::PLSNCB2bBrowserSettings *ui;
	QString serviceName;
	QList<PLSNCB2bBrowserSettingData> cacheSelectedDatas;
	int seletedNumbers = 0;
	bool requestExisted{false};
};

#endif // PLSNCB2BBROWSERSETTINGS_H
