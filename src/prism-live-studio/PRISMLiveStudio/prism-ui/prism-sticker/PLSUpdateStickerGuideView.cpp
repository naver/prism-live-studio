#include "PLSUpdateStickerGuideView.h"
#include "ui_PLSUpdateStickerGuideView.h"
#include "utils-api.h"
#include "libui.h"

PLSUpdateStickerGuideView::PLSUpdateStickerGuideView(QWidget *buddy) : QFrame(buddy)
{
	pls_uistep_v2_set_custom_show_hide_name(this, "Update Sticker Source Guide View");
	setAttribute(Qt::WA_NativeWindow);
	ui = pls_new<Ui::PLSUpdateStickerGuideView>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSUpdateStickerGuideView"});
	ui->updateStickerGuideTitle->setText(tr("main.prism.update.sticker.guide.text"));
	connect(ui->updateStickerOkButton, &QPushButton::clicked, this, [this] { emit onFinishButtonClicked(); });
	ui->updateStickerOkButton->setText(tr("main.prism.update.sticker.guide.ok.button"));
	// Enable word wrap for long text
	ui->updateStickerGuideTitle->setWordWrap(true);
	ui->updateStickerGuideTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	// Let the frame height adapt to content
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

PLSUpdateStickerGuideView::~PLSUpdateStickerGuideView() {}

void PLSUpdateStickerGuideView::updateGuideIcon(const QPixmap &pix)
{
	if (pix.isNull()) {
		setDefaultIcon(true);
		return;
	}
	setDefaultIcon(false);
	QPixmap scaled = pix.scaled(ui->updateStickerGuideIcon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	ui->updateStickerGuideIcon->setPixmap(scaled);
}

void PLSUpdateStickerGuideView::updateOkButtonEnabled(bool enabled)
{
	ui->updateStickerOkButton->setEnabled(enabled);
	pls_flush_style(ui->updateStickerOkButton);
}

void PLSUpdateStickerGuideView::setDefaultIcon(bool defaultIcon)
{
	ui->updateStickerGuideIcon->setProperty("useDefault", defaultIcon);
	if (defaultIcon) {
		ui->updateStickerGuideIcon->setPixmap(QPixmap());
	}
	pls_flush_style(ui->updateStickerGuideIcon);
}

void PLSUpdateStickerGuideView::calcFixedHeight()
{
	// Calculate text height using QFontMetrics
	int textWidth = ui->updateStickerGuideTitle->width();
	QFontMetrics fm(ui->updateStickerGuideTitle->font());
	QRect boundingRect = fm.boundingRect(QRect(0, 0, textWidth, 0), Qt::TextWordWrap | Qt::AlignLeft, ui->updateStickerGuideTitle->text());

	int textHeight = boundingRect.height();
	constexpr int TOP_PADDING = 9;
	constexpr int BOTTOM_PADDING = 11;
	constexpr int BORDER_HEIGHT = 2;

	setFixedHeight(textHeight + TOP_PADDING + BOTTOM_PADDING + BORDER_HEIGHT);
}

void PLSUpdateStickerGuideView::showEvent(QShowEvent *event)
{
	calcFixedHeight();
	QFrame::showEvent(event);
}
