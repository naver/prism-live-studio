#ifndef PLSANIMATIONLABEL_H
#define PLSANIMATIONLABEL_H

#include "libui-globals.h"

#include <QLabel>
#include <QMediaPlayer>
#include <QVideoFrame>
#include <QVideoSink>
#include <functional>

using LoadCallback = std::function<void()>;

class LIBUI_API PLSAnimationLabel : public QLabel {
	Q_OBJECT
public:
	explicit PLSAnimationLabel(QWidget *parent = nullptr);
	~PLSAnimationLabel() override;

	void play(const QString &videoPath, bool loop = true);
	void stop();
	void setLoadCallback(LoadCallback f) { m_loadCallback = std::move(f); }

public slots:
	void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
	void onVideoFrameChanged(const QVideoFrame &frame);

signals:
	void loadFinished();
	void stopped();
	void playFinished();

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	QMediaPlayer *m_mediaPlayer = nullptr;
	QVideoSink *m_videoSink = nullptr;
	bool m_isStop = false;
	LoadCallback m_loadCallback;
	QVideoFrame m_videoFrame;
};

#endif // PLSANIMATIONLABEL_H
