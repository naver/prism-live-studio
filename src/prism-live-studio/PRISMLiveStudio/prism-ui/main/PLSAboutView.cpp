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
	setFixedSize(342, 360);
	initSize(342, 360);
	ui->versionLabel->setText(QString("%1.%2.%3 (Build %4)").arg(PRISM_VERSION_MAJOR).arg(PRISM_VERSION_MINOR).arg(PRISM_VERSION_PATCH).arg(PRISM_VERSION_BUILD));
	bool disabled = pls_is_output_actived();
	ui->checkUpdateButton->setDisabled(disabled);
	pls_uistep_v2_set_title(this, QStringLiteral("About"));

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
