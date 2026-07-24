#include "PLSMediaRender.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QThread>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>

#include "libui.h"
#include "log/log.h"
#include "PLSSceneTemplateContainer.h"

PLSMediaRender::PLSMediaRender(QWidget *parent) : QWidget{parent}
{
	m_videoSink.reset(new QVideoSink(this));
	connect(m_videoSink.data(), &QVideoSink::videoFrameChanged, this, &PLSMediaRender::onVideoFrame);
	connect(m_videoSink.data(), &QVideoSink::videoFrameChanged, this, qOverload<>(&PLSMediaRender::update));

	imageLabel = new PLSSceneTemplateBorderLabel(this);
	imageLabel->setObjectName("imageLabel");

	auto boxLayout = new QHBoxLayout(this);
	boxLayout->setContentsMargins(QMargins());
	boxLayout->addWidget(imageLabel);
}

PLSMediaRender::~PLSMediaRender()
{
	if (nullptr != m_mediaPlayer) {
		m_mediaPlayer->stop();
		m_mediaPlayer = nullptr;
	}
}

void PLSMediaRender::setMediaPlayer(QMediaPlayer *mediaPlayer)
{
	if (nullptr != m_mediaPlayer) {
		m_mediaPlayer->setVideoOutput(nullptr);
		m_mediaPlayer->stop();
	}

	m_frameVideo = {};

	m_mediaPlayer = mediaPlayer;
	m_mediaPlayer->setVideoOutput(m_videoSink.data());
}

void PLSMediaRender::setHasBorder(bool bBorder)
{
	imageLabel->setHasBorder(bBorder);
}

void PLSMediaRender::setSceneName(const QString &name)
{
	imageLabel->setSceneNameLabel(name);
}

void PLSMediaRender::onVideoFrame(const QVideoFrame &frame)
{
	m_frameVideo = frame;
}

void PLSMediaRender::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		PLS_UI_ACTION("In Scene Template Window, the scene template video thumbnail has been clicked.");
		emit clicked();
	}
}

void PLSMediaRender::showAIBadge(const QPixmap &pixmap, bool bLongAIBadge)
{
	imageLabel->showAIBadge(pixmap, bLongAIBadge);
}

void PLSMediaRender::showPlusBadge(const QPixmap &pixmap)
{
	imageLabel->showPlusBadge(pixmap);
}

void PLSMediaRender::setDefaultBgImagePath(const QString &path)
{
	m_defaultBgImagePath = path;
	repaint();
}

void PLSMediaRender::setDefaultBgImagePixmap(const QPixmap &pixmap)
{
	m_defaultBgImagePixmap = pixmap;
	repaint();
}

void PLSMediaRender::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	const QSize widgetSize = size();
	painter.fillRect(rect(), QColor("#000000"));

	auto scaleMode = property("keepAspectRatioByExpanding").toBool() ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
	if (m_frameVideo.isValid() && m_frameVideo.width() > 0 && m_frameVideo.height() > 0) {
		QImage frameImage = m_frameVideo.toImage();
		if (!frameImage.isNull() && frameImage.width() > 0 && frameImage.height() > 0) {
			QSize scaledSize = frameImage.size().scaled(widgetSize, scaleMode);
			QRect targetRect(QPoint((width() - scaledSize.width()) / 2, (height() - scaledSize.height()) / 2), scaledSize);
			m_frameVideo.paint(&painter, targetRect, {});
			return;
		}
	}

	QPixmap pixmap;
	if (!m_defaultBgImagePixmap.isNull()) {
		pixmap = m_defaultBgImagePixmap;
	} else if (!m_defaultBgImagePath.isEmpty()) {
		QPixmap bgPixmap(m_defaultBgImagePath);
		if (!bgPixmap.isNull()) {
			pixmap = bgPixmap;
		}
	}

	if (pixmap.isNull()) {
		if (m_defaultPixmap.isNull()) {
			m_defaultPixmap = pls_load_pixmap(QString(":resource/images/scene-template/BG.png"), widgetSize);
		}
		pixmap = m_defaultPixmap;
	}

	if (!pixmap.isNull()) {
		QSize scaledSize = pixmap.size().scaled(widgetSize, scaleMode);
		QRect targetRect(QPoint((width() - scaledSize.width()) / 2, (height() - scaledSize.height()) / 2), scaledSize);
		painter.drawPixmap(targetRect, pixmap);
	}
}
