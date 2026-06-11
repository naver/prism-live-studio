#include "PLSAboutView.hpp"
#include "ui_PLSAboutView.h"
#include "ui-config.h"
#include "pls-common-language.hpp"
#include "utils-api.h"
#include "frontend-api.h"
#include "prism-version.h"

PLSAboutView::PLSAboutView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSAboutView>();
	setResizeEnabled(false);
	pls_set_css(this, {"PLSAboutView"});
	setupUi(ui);
	initSize(342, 360);
	QString version = QString::asprintf("%s %s", tr(ABOUT_CURRENT_VERSION).toUtf8().constData(), PRISM_VERSION);
	ui->versionLabel->setText(version);
	bool disabled = pls_is_output_actived();
	ui->checkUpdateButton->setDisabled(disabled);

#if defined(Q_OS_MACOS)
	ui->verticalSpacer_2->changeSize(0, 51, QSizePolicy::Fixed, QSizePolicy::Fixed);
	setWindowTitle(tr("Mac.Title.About"));
#endif
}

PLSAboutView::~PLSAboutView()
{
	pls_delete(ui);
}

void PLSAboutView::on_checkUpdateButton_clicked()
{
	this->accept();
}
