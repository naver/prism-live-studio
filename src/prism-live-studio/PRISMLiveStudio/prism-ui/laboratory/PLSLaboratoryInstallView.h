#ifndef PLSLABORATORYINSTALLVIEW_H
#define PLSLABORATORYINSTALLVIEW_H

#include "PLSDialogView.h"
#include "loading-event.hpp"
#include "cancel.hpp"

extern const int g_installSuccess;
extern const int g_installNetworkError;
extern const int g_installUnkownError;

namespace Ui {
class PLSLaboratoryInstallView;
}

class PLSLaboratoryInstallView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLaboratoryInstallView(const QString &labId, QWidget *parent = nullptr);
	~PLSLaboratoryInstallView() override;

private:
	void showLoading();
	void hideLoading();
	void startInstall();

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event) override;

private:
	Ui::PLSLaboratoryInstallView *ui;
	PLSLoadingEvent m_loadingEvent;
	bool m_installing{false};
	QString m_labId;
	PLSCancel m_plsCancel;
	int returnCode = 0;
};

#endif // PLSLABORATORYINSTALLVIEW_H
