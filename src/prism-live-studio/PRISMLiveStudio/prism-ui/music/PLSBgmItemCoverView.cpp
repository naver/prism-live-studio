#include "PLSBgmItemCoverView.h"
#include "ui_PLSBgmItemCoverView.h"

#include <QPainter>
#include <QPen>
#include <QMovie>
#include <QGraphicsBlurEffect>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainterPath>

#include "liblog.h"
#include "log/module_names.h"
#include "action.h"
#include "pls-common-define.hpp"
#include "PLSScrollingLabel.h"
#include "utils-api.h"
using namespace common;
PLSBgmItemCoverView::PLSBgmItemCoverView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSBgmItemCoverView>();
	ui->setupUi(this);

	blurEffect = pls_new<QGraphicsBlurEffect>(ui->backgroundLabel);
	blurEffect->setBlurRadius(90 * this->width() / 300);

	opacityEffect = pls_new<QGraphicsOpacityEffect>(this);
	opacityEffect->setOpacity(0.4);

	ui->backgroundLabel->setGraphicsEffect(opacityEffect);
	ui->backgroundLabel->setGraphicsEffect(blurEffect);

	imageLabel = pls_new<PLSBgmItemCoverImage>(this);
	imageLabel->SetRoundedRect(true);

	auto shadowEffect = pls_new<QGraphicsDropShadowEffect>(this);
	shadowEffect->setOffset(0, 0);
	shadowEffect->setColor(QColor(0, 0, 0, 128));
	shadowEffect->setBlurRadius(5);
	auto opacityEffect_ = pls_new<QGraphicsOpacityEffect>(this);
	imageLabel->setGraphicsEffect(opacityEffect_);
	opacityEffect_->setOpacity(0.2);
	imageLabel->setGraphicsEffect(shadowEffect);
	imageLabel->setObjectName("imageLabel");

	titleFrame = pls_new<QFrame>(this);
	titleFrame->setObjectName("titleFrame");
	auto hTitleLayout = pls_new<QHBoxLayout>(titleFrame);
	hTitleLayout->setContentsMargins(20, 0, 20, 0);

	titleLabel = pls_new<PLSScrollingLabel>(this);
	titleLabel->setAlignment(Qt::AlignCenter);
	titleLabel->setObjectName("titleLabel");
	hTitleLayout->addWidget(titleLabel);

	producerFrame = pls_new<QFrame>(this);
	producerFrame->setObjectName("producerFrame");
	auto hProducerLayout = pls_new<QHBoxLayout>(producerFrame);
	hProducerLayout->setContentsMargins(20, 0, 20, 0);

	producerLabel = pls_new<PLSScrollingLabel>(this);
	producerLabel->setAlignment(Qt::AlignCenter);
	producerLabel->setObjectName("producerLabel");
	hProducerLayout->addWidget(producerLabel);
	auto proOpacityEffect = pls_new<QGraphicsOpacityEffect>(this);
	producerLabel->setGraphicsEffect(proOpacityEffect);
	proOpacityEffect->setOpacity(0.6);

	playingLabel = pls_new<QLabel>(imageLabel);
	playingLabel->hide();
	playingLabel->setObjectName("playingLabel");

	movie = pls_new<QMovie>(BGM_MUSIC_PLAYING_GIF);
	movie->setScaledSize(QSize(16, 16));
	playingLabel->setMovie(movie);
}

PLSBgmItemCoverView::~PLSBgmItemCoverView()
{
	if (movie) {
		movie->deleteLater();
		movie = nullptr;
	}
	pls_delete(ui);
}

void PLSBgmItemCoverView::SetMusicInfo(const QString &title_, const QString &producer_)
{
	if (!titleLabel || !producerLabel) {
		return;
	}

	titleLabel->SetText(title_);
	producerLabel->SetText(producer_);
}

void PLSBgmItemCoverView::SetCoverPath(const QString &coverPath_, bool)
{
	if (coverPath_.isEmpty()) {
		return;
	}

	this->coverPath = coverPath_;
	QPixmap pixmap(coverPath_);
	ui->backgroundLabel->SetPixmap(pixmap);
	if (imageLabel) {
		imageLabel->SetPixmap(pixmap);
	}
	ResizeUI();
}

void PLSBgmItemCoverView::SetImage(const QImage &image)
{
	ui->backgroundLabel->SetPixmap(QPixmap::fromImage(image));
	if (imageLabel) {
		imageLabel->SetPixmap(QPixmap::fromImage(image));
	}
	ResizeUI();
}

void PLSBgmItemCoverView::ShowPlayingGif(bool show)
{
	if (!movie || !playingLabel) {
		return;
	}
	show ? movie->start() : movie->stop();
	playingLabel->setVisible(show);

	QMetaObject::invokeMethod(
		this,
		[this]() {
			if (imageLabel) {
				int margin = 7;
				playingLabel->setGeometry(margin, imageLabel->height() - margin - playingLabel->height(), playingLabel->width(), playingLabel->height());
			}
		},
		Qt::QueuedConnection);
}

void PLSBgmItemCoverView::DpiChanged(double)
{
	if (movie) {
		movie->setScaledSize(QSize(16, 16));
	}
}

void PLSBgmItemCoverView::resizeEvent(QResizeEvent *event)
{
	QMetaObject::invokeMethod(this, "ResizeUI", Qt::QueuedConnection);
	QFrame::resizeEvent(event);
}

void PLSBgmItemCoverView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		if (event->pos().x() < 10 || event->pos().x() > width() - 10) {
			QFrame::mousePressEvent(event);
			return;
		}
		startPoint = event->pos();
		mousePressed = true;
	}
}

void PLSBgmItemCoverView::mouseMoveEvent(QMouseEvent *event)
{
	QFrame::mouseMoveEvent(event);
	if (mousePressed) {
		emit CoverPressed(event->pos() - startPoint);
	}
}

void PLSBgmItemCoverView::mouseReleaseEvent(QMouseEvent *event)
{
	mousePressed = false;
	QFrame::mouseReleaseEvent(event);
}

void PLSBgmItemCoverView::ResizeUI()
{
	if (blurEffect) {
		blurEffect->setBlurRadius(90 * this->width() / 300);
	}

	if (imageLabel) {
		imageLabel->setGeometry((this->width() - imageLabel->width()) / 2, 35, imageLabel->width(), imageLabel->height());
	}
	if (titleFrame) {
		titleFrame->setGeometry(0, 180, this->width(), titleLabel->height());
	}
	if (producerFrame) {
		producerFrame->setGeometry(0, 209, this->width(), producerLabel->height());
	}
	if (playingLabel) {
		QMetaObject::invokeMethod(
			this,
			[this]() {
				if (imageLabel) {
					int margin = 7;
					playingLabel->setGeometry(margin, imageLabel->height() - margin - playingLabel->height(), playingLabel->width(), playingLabel->height());
				}
			},
			Qt::QueuedConnection);
	}
}

PLSBgmItemCoverImage::PLSBgmItemCoverImage(QWidget *parent) : QLabel(parent) {}

void PLSBgmItemCoverImage::SetPixmap(const QPixmap &pixmap_)
{
	this->pixmap = pixmap_;
	update();
}

void PLSBgmItemCoverImage::SetRoundedRect(bool set)
{
	setRoundedRect = set;
}

void PLSBgmItemCoverImage::paintEvent(QPaintEvent *event)
{
	QFrame::paintEvent(event);
	QPainter painter(this);

	if (!pixmap.isNull()) {
		painter.setRenderHints(QPainter::Antialiasing, true);
		if (setRoundedRect) {
			QPainterPath painterPath;
			painterPath.addRoundedRect(this->rect(), 3.0, 3.0);
			painter.setClipPath(painterPath);
			painter.drawPixmap(painterPath.boundingRect().toRect(), pixmap.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			painter.setPen(Qt::NoPen);
			painter.drawPath(painterPath);
		} else {
			painter.drawPixmap(this->rect(), pixmap);
		}
	}
}
