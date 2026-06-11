#include "PLSShoppingRhythmicity.h"
#include "ui_PLSShoppingRhythmicity.h"
#include <QDir>
#include <util/util.hpp>
#include <util/platform.h>
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"
#include "libui.h"

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

PLSShoppingRhythmicity::PLSShoppingRhythmicity(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSShoppingRhythmicity>();
	pls_set_css(this, {"PLSShoppingRhythmicity"});
	setupUi(ui);
	auto lang = pls_get_current_language();
	if (IS_KR()) {
#if defined(Q_OS_MACOS)
		setFixedSize(410, 400);
#else
		setFixedSize(410, 440);
#endif
	} else if (pls_is_match_current_language(QLocale::Indonesian)) {
#if defined(Q_OS_MACOS)
		setFixedSize(410, 460);
#else
		setFixedSize(410, 500);
#endif
	} else {
#if defined(Q_OS_MACOS)
		setFixedSize(410, 434);
#else
		setFixedSize(410, 474);
#endif
	}
	ui->topTitleLabel->setProperty("lang", lang);
	ui->subTitleLabel->setProperty("lang", lang);
	ui->agreeButton->setProperty("lang", lang);
	ui->declineButton->setProperty("lang", lang);
	setWindowTitle(tr("Alert.Title"));
	setResizeEnabled(false);
	ui->topTitleLabel->setText(setLineHeight(tr("navershopping.liveinfo.rhythmicit.title"), 26));
	ui->subTitleLabel->setText(setLineHeight(tr("navershopping.liveinfo.rhythmicit.subtitle"), 18));
	createTwoLineButton(ui->agreeButton, tr("navershopping.liveinfo.rhythmicit.accept.title"), tr("navershopping.liveinfo.rhythmicit.accept.subtitle"));
	createTwoLineButton(ui->declineButton, tr("navershopping.liveinfo.rhythmicit.decline.title"), tr("navershopping.liveinfo.rhythmicit.decline.subtitle"));

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

void PLSShoppingRhythmicity::closeEvent(QCloseEvent *event)
{
#if defined(Q_OS_WIN)
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		event->ignore();
		return;
	}
#endif
	PLSDialogView::closeEvent(event);
}

void PLSShoppingRhythmicity::createTwoLineButton(QPushButton *button, const QString &line1, const QString &line2) const
{
	auto label1 = pls_new<QLabel>();
	label1->setText(line1);
	label1->setAlignment(Qt::AlignCenter);
	label1->setObjectName("buttonLine1");
	auto label2 = pls_new<QLabel>();
	label2->setText(line2);
	label2->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
	label2->setObjectName("buttonLine2");
	auto vl = pls_new<QVBoxLayout>();
	vl->setContentsMargins(8, 10, 8, 0);
	vl->setSpacing(3);
	vl->addWidget(label1);
	vl->addWidget(label2);
	vl->addStretch();
	button->setLayout(vl);
}

QString PLSShoppingRhythmicity::setLineHeight(QString sourceText, uint lineHeight) const
{
	QString sourceTemplate = "<p style=\"line-height:%1px\">%2<p>";
	QString text = sourceText.replace('\n', "<br/>");
	QString targetText = sourceTemplate.arg(lineHeight).arg(text);
	return targetText;
}

void PLSShoppingRhythmicity::on_agreeButton_clicked()
{
	this->accept();
}

void PLSShoppingRhythmicity::on_declineButton_clicked()
{
	this->reject();
}

PLSShoppingRhythmicity::~PLSShoppingRhythmicity()
{
	pls_delete(ui);
}

void PLSShoppingRhythmicity::setURL(const QString &url)
{
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(url)
								 .initBkgColor(QColor(17, 17, 17))
								 .css("html, body { background-color: #111111; }")
								 .showAtLoadEnded(true));
	ui->verticalLayout_2->addWidget(m_browserWidget);
}
