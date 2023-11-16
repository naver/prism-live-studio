#ifndef PLSLABORATORY_H
#define PLSLABORATORY_H

#include "PLSDialogView.h"
#include <libbrowser.h>
#include <QButtonGroup>
namespace Ui {
class PLSLaboratory;
}

class PLSLaboratory : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLaboratory(QWidget *parent = nullptr);
	~PLSLaboratory() override;
	void changeCheckedState(const QString &labId, bool used);
	bool getCheckedState(const QString &labId) const;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void setupListWidgetItems();
	void refreshCurrentItemSelected();
	void setDetailInfo(const QString &labId);
	void openLab(const QString &labId);
	void closeLab(const QString &labId);
	void setupSendJsonObject(QJsonObject &jsonObject) const;
	void receiveWebViewJsMessage(const QJsonObject &msg) const;
	bool isOpenButtonChecked() const;
	void checkInstallFinished(const QString &labId, bool installPageEnter);
	bool checkInstallFinishedRestart(const QString &labId, bool installPageEnter);
	QJsonObject getDetailPageButtonEnabledJsEvent(const QString &url) const;

private slots:
	void on_closeButton_clicked();
	void on_openButton_clicked();
	void labItemChanged(QAbstractButton *button);

private:
	Ui::PLSLaboratory *ui;
	pls::browser::BrowserWidget *m_browserWidget = nullptr;
	QButtonGroup m_buttonGroup;
};

#endif // PLSLABORATORY_H
