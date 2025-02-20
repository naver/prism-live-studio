#pragma once

#include <mutex>

#include <QPointer>
#include <QScopedPointer>

#include <QPixmap>

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>

#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>

class PLSMediaRender : public QOpenGLWidget, protected QOpenGLFunctions {
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

signals:
	void clicked();

protected slots:
	void onVideoFrame(const QVideoFrame &frame);

protected:
	void initializeGL() override;
	void resizeGL(int w, int h) override;

	bool uploadFrame();
	void renderFrame();

	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	QScopedPointer<QOpenGLShaderProgram> m_shaderProgram;
	QVideoFrameFormat::PixelFormat m_programPixelFormat = QVideoFrameFormat::Format_Invalid;
	QScopedPointer<QOpenGLBuffer> m_bufferVertex;
	QScopedPointer<QOpenGLTexture> m_glTextureY;
	QScopedPointer<QOpenGLTexture> m_glTextureU;
	QScopedPointer<QOpenGLTexture> m_glTextureV;
	QPointer<QMediaPlayer> m_mediaPlayer;
	QScopedPointer<QVideoSink> m_videoSink;
	std::mutex m_frameMutex;
	QVideoFrame m_frameVideo;
	QVideoFrameFormat::PixelFormat m_framePixelFormat = QVideoFrameFormat::Format_Invalid;

	bool m_bBorder = false;
	QString m_strName;

	bool m_bOpenGLFailed = false;

	const QPixmap *m_pPixmapAIBadge = nullptr;
	bool m_bLongAIBadge = false;
};
