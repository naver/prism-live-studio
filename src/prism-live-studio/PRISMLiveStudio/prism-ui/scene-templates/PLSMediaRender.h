#pragma once

#include <mutex>

#include <QPointer>
#include <QScopedPointer>

#include <QPixmap>

#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>

#include "PLSSceneTemplateBorderLabel.h"

class PLSMediaRender : public QWidget {
	Q_OBJECT
public:
	explicit PLSMediaRender(QWidget *parent = nullptr);
	~PLSMediaRender();

public:
	void setMediaPlayer(QMediaPlayer *mediaPlayer);
	QMediaPlayer *getMediaPlayer() const { return m_mediaPlayer; }

	void setHasBorder(bool bBorder);
	void setSceneName(const QString &name);
	void showAIBadge(const QPixmap &pixmap, bool bLongAIBadge);
	void showPlusBadge(const QPixmap &pixmap);

	void setDefaultBgImagePath(const QString &path);
	void setDefaultBgImagePixmap(const QPixmap &pixmap);

signals:
	void clicked();

protected slots:
	void onVideoFrame(const QVideoFrame &frame);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QPointer<QMediaPlayer> m_mediaPlayer;
	QScopedPointer<QVideoSink> m_videoSink;
	QVideoFrame m_frameVideo;
	QString m_defaultBgImagePath;
	QPixmap m_defaultBgImagePixmap;
	QPixmap m_defaultPixmap;
	PLSSceneTemplateBorderLabel *imageLabel;
};
