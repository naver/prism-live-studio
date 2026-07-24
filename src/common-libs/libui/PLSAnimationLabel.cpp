#include "PLSAnimationLabel.h"

#include <QFileInfo>
#include <QPainter>

#include "liblog.h"

#define ANIMATIONMODULE "PLSAnimationLabel"

PLSAnimationLabel::PLSAnimationLabel(QWidget *parent) : QLabel(parent)
{
	m_mediaPlayer = new QMediaPlayer(this);
	m_videoSink = new QVideoSink(this);

	m_mediaPlayer->setVideoOutput(m_videoSink);

	connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &PLSAnimationLabel::onMediaStatusChanged);
	connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &PLSAnimationLabel::onVideoFrameChanged);

	setScaledContents(true);
	setAlignment(Qt::AlignCenter);
}

PLSAnimationLabel::~PLSAnimationLabel()
{
	stop();
}

void PLSAnimationLabel::play(const QString &videoPath, bool loop)
{
	PLS_INFO(ANIMATIONMODULE, "Playing video: %s", qPrintable(videoPath));

	QUrl sourceUrl;
	if (videoPath.startsWith("qrc:/")) {
		sourceUrl = QUrl(videoPath);
	} else if (videoPath.startsWith(":/")) {
		sourceUrl = QUrl(QStringLiteral("qrc%1").arg(videoPath));
	} else {
		QFileInfo fileInfo(videoPath);
		if (fileInfo.isAbsolute() || fileInfo.exists()) {
			sourceUrl = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
		} else {
			sourceUrl = QUrl(videoPath);
		}
	}

	m_isStop = false;
	m_mediaPlayer->setSource(sourceUrl);
	m_mediaPlayer->setLoops(loop ? -1 : 1);
	m_mediaPlayer->play();

	PLS_INFO(ANIMATIONMODULE, "Video playback started, loop: %s", pls_bool_2_string(loop));
}

void PLSAnimationLabel::stop()
{
	const bool wasRunning = m_mediaPlayer && !m_mediaPlayer->source().isEmpty() &&
				m_mediaPlayer->playbackState() != QMediaPlayer::StoppedState;

	if (m_mediaPlayer) {
		m_mediaPlayer->stop();
	}
	m_isStop = true;
	m_videoFrame = {};
	update();
	if (wasRunning) {
		emit stopped();
	}
}

void PLSAnimationLabel::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
	PLS_INFO(ANIMATIONMODULE, "MediaStatus changed: %d", static_cast<int>(status));

	switch (status) {
	case QMediaPlayer::LoadedMedia:
		emit loadFinished();
		if (m_loadCallback) {
			m_loadCallback();
		}
		break;
	case QMediaPlayer::InvalidMedia:
		PLS_INFO(ANIMATIONMODULE, "Invalid media: %s", qPrintable(m_mediaPlayer->errorString()));
		break;
	case QMediaPlayer::EndOfMedia:
		emit playFinished();
		break;
	default:
		break;
	}
}

void PLSAnimationLabel::onVideoFrameChanged(const QVideoFrame &frame)
{
	if (m_isStop || !frame.isValid()) {
		return;
	}

	m_videoFrame = frame;
	update();
}

void PLSAnimationLabel::paintEvent(QPaintEvent *e)
{
	QLabel::paintEvent(e);

	if (!m_videoFrame.isValid()) {
		return;
	}

	QPainter painter(this);
	QVideoFrame::PaintOptions options = {};
	m_videoFrame.paint(&painter, QRect({0, 0}, rect().size()), options);
}
