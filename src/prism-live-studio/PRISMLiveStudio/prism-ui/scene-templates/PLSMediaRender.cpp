#include "PLSMediaRender.h"
#include "PLSMediaRenderShader.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QThread>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>

#include "libui.h"
#include "log/log.h"
#include "PLSSceneTemplateContainer.h"

PLSMediaRender::PLSMediaRender(QWidget *parent) : QOpenGLWidget{parent}
{
	m_videoSink.reset(new QVideoSink(this));

	connect(m_videoSink.data(), &QVideoSink::videoFrameChanged, this, &PLSMediaRender::onVideoFrame, Qt::DirectConnection);
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

	makeCurrent();
	m_shaderProgram.reset();
	m_bufferVertex.reset();
	m_glTextureY.reset();
	m_glTextureU.reset();
	m_glTextureV.reset();
	doneCurrent();
}

void PLSMediaRender::initializeGL()
{
	initializeOpenGLFunctions();

	float vertexData[] = {-1, 1, 0, 0, -1, -1, 0, 1, 1, 1, 1, 0, 1, -1, 1, 1};

	m_bufferVertex.reset(new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer));
	if (m_bufferVertex->create() && m_bufferVertex->bind()) {
		m_bufferVertex->allocate(vertexData, sizeof(vertexData));
	}

	m_glTextureY.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
	m_glTextureU.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
	m_glTextureV.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));

	glClearColor(0, 0, 0, 1);
}

void PLSMediaRender::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
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
	std::lock_guard guard(m_frameMutex);

	m_frameVideo = frame;
}

bool PLSMediaRender::uploadFrame()
{
	m_frameMutex.lock();
	auto frame = m_frameVideo;
	m_frameMutex.unlock();

	if (!frame.isValid() || frame.width() < 2 || frame.height() < 2) {
		return false;
	}

	m_framePixelFormat = frame.pixelFormat();

	if (QVideoFrameFormat::Format_NV12 == frame.pixelFormat()) {
		if (m_glTextureY->width() != frame.width() || m_glTextureY->height() != frame.height()) {
			m_glTextureY->destroy();
			m_glTextureU->destroy();
		}
		if (!m_glTextureY->isStorageAllocated()) {
			m_glTextureY->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureY->setSize(frame.width(), frame.height());
			m_glTextureY->allocateStorage();

			m_glTextureU->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureU->setSize(frame.width() / 2, frame.height() / 2);
			m_glTextureU->allocateStorage();
		}

		if (m_glTextureY->isStorageAllocated() && m_glTextureU->isStorageAllocated()) {
			frame.map(QVideoFrame::ReadOnly);
			m_glTextureY->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.bits(0));
			m_glTextureY->generateMipMaps();
			m_glTextureU->setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8, frame.bits(1));
			m_glTextureU->generateMipMaps();
			frame.unmap();
		}
	} else if (QVideoFrameFormat::Format_YUV420P == frame.pixelFormat()) {
		if (m_glTextureY->width() != frame.width() || m_glTextureY->height() != frame.height()) {
			m_glTextureY->destroy();
			m_glTextureU->destroy();
			m_glTextureV->destroy();
		}
		if (!m_glTextureY->isStorageAllocated()) {
			m_glTextureY->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureY->setSize(frame.width(), frame.height());
			m_glTextureY->allocateStorage();

			m_glTextureU->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureU->setSize(frame.width() / 2, frame.height() / 2);
			m_glTextureU->allocateStorage();
			m_glTextureV->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureV->setSize(frame.width() / 2, frame.height() / 2);
			m_glTextureV->allocateStorage();
		}

		if (m_glTextureY->isStorageAllocated() && m_glTextureU->isStorageAllocated() && m_glTextureV->isStorageAllocated()) {
			frame.map(QVideoFrame::ReadOnly);
			m_glTextureY->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.bits(0));
			m_glTextureY->generateMipMaps();
			m_glTextureU->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.bits(1));
			m_glTextureU->generateMipMaps();
			m_glTextureV->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.bits(2));
			m_glTextureV->generateMipMaps();
			frame.unmap();
		}
	} else if (QVideoFrameFormat::Format_BGRA8888 == frame.pixelFormat()) {
		if (m_glTextureY->width() != frame.width() || m_glTextureY->height() != frame.height()) {
			m_glTextureY->destroy();
		}
		if (!m_glTextureY->isStorageAllocated()) {
			m_glTextureY->setFormat(QOpenGLTexture::RGBA8_UNorm);
			m_glTextureY->setSize(frame.width(), frame.height());
			m_glTextureY->allocateStorage();
		}

		if (m_glTextureY->isStorageAllocated()) {
			frame.map(QVideoFrame::ReadOnly);
			m_glTextureY->setData(QOpenGLTexture::BGRA, QOpenGLTexture::UInt8, frame.bits(0));
			m_glTextureY->generateMipMaps();
			frame.unmap();
		}
	} else {
		return false;
	}

	return true;
}

void PLSMediaRender::paintGL()
{
	if (!isValid()) {
		if (!m_bOpenGLFailed) {
			PLS_WARN(SCENE_TEMPLATE, "%p-%s: OpenGL functions is not initialized", this, __FUNCTION__);
			m_bOpenGLFailed = true;
		}
		return;
	} else if (m_bOpenGLFailed) {
		PLS_WARN(SCENE_TEMPLATE, "%p-%s: OpenGL functions is initialized", this, __FUNCTION__);
		m_bOpenGLFailed = false;
	}

	glClear(GL_COLOR_BUFFER_BIT);

	if (!m_bufferVertex->isCreated()) {
		return;
	}
	m_bufferVertex->bind();

	glEnable(GL_TEXTURE_2D);

	if (!uploadFrame()) {
		return;
	}

	if (QVideoFrameFormat::Format_NV12 == m_framePixelFormat) {
		if (m_programPixelFormat != m_framePixelFormat && m_shaderProgram) {
			m_shaderProgram.reset();
		}
		if (!m_shaderProgram) {
			m_programPixelFormat = m_framePixelFormat;
			m_shaderProgram.reset(new QOpenGLShaderProgram(this));
			if (m_shaderProgram->create()) {
				if (m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, SHADER_FOR_VERTEX) &&
				    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SHADER_FOR_NV12)) {
					m_shaderProgram->link();
				}
			}
		}

		m_shaderProgram->bind();

		QMatrix4x4 mat4;
		if (static_cast<float>(width()) / height() < static_cast<float>(m_glTextureY->width()) / m_glTextureY->height()) {
			auto dstHeight = static_cast<float>(width()) * m_glTextureY->height() / m_glTextureY->width() / height();
			mat4.scale(1, dstHeight);
		} else {
			auto dstWidth = static_cast<float>(height()) * m_glTextureY->width() / m_glTextureY->height() / width();
			mat4.scale(dstWidth, 1);
		}
		m_shaderProgram->setUniformValue("matFullScreen", mat4.transposed());

		m_shaderProgram->enableAttributeArray("position");
		m_shaderProgram->setAttributeBuffer("position", GL_FLOAT, 0, 2, sizeof(float) * 4);
		m_shaderProgram->enableAttributeArray("texcoord");
		m_shaderProgram->setAttributeBuffer("texcoord", GL_FLOAT, sizeof(float) * 2, 2, sizeof(float) * 4);

		if (m_shaderProgram->isLinked()) {
			if (m_glTextureY && m_glTextureY->isCreated()) {
				m_glTextureY->bind(0);
				m_shaderProgram->setUniformValue("textureY", 0);
			}
			if (m_glTextureU && m_glTextureU->isCreated()) {
				m_glTextureU->bind(1);
				m_shaderProgram->setUniformValue("textureUV", 1);
			}
		}
	} else if (QVideoFrameFormat::Format_YUV420P == m_framePixelFormat) {
		if (m_programPixelFormat != m_framePixelFormat && m_shaderProgram) {
			m_shaderProgram.reset();
		}
		if (!m_shaderProgram) {
			m_programPixelFormat = m_framePixelFormat;
			m_shaderProgram.reset(new QOpenGLShaderProgram(this));
			if (m_shaderProgram->create()) {
				if (m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, SHADER_FOR_VERTEX) &&
				    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SHADER_FOR_YUV420P)) {
					m_shaderProgram->link();
				}
			}
		}

		m_shaderProgram->bind();

		QMatrix4x4 mat4;
		if (static_cast<float>(width()) / height() < static_cast<float>(m_glTextureY->width()) / m_glTextureY->height()) {
			auto dstHeight = static_cast<float>(width()) * m_glTextureY->height() / m_glTextureY->width() / height();
			mat4.scale(1, dstHeight);
		} else {
			auto dstWidth = static_cast<float>(height()) * m_glTextureY->width() / m_glTextureY->height() / width();
			mat4.scale(dstWidth, 1);
		}
		m_shaderProgram->setUniformValue("matFullScreen", mat4.transposed());

		m_shaderProgram->enableAttributeArray("position");
		m_shaderProgram->setAttributeBuffer("position", GL_FLOAT, 0, 2, sizeof(float) * 4);
		m_shaderProgram->enableAttributeArray("texcoord");
		m_shaderProgram->setAttributeBuffer("texcoord", GL_FLOAT, sizeof(float) * 2, 2, sizeof(float) * 4);

		if (m_shaderProgram->isLinked()) {
			if (m_glTextureY && m_glTextureY->isCreated()) {
				m_glTextureY->bind(0);
				m_shaderProgram->setUniformValue("textureY", 0);
			}
			if (m_glTextureU && m_glTextureU->isCreated()) {
				m_glTextureU->bind(1);
				m_shaderProgram->setUniformValue("textureU", 1);
			}
			if (m_glTextureV && m_glTextureV->isCreated()) {
				m_glTextureV->bind(2);
				m_shaderProgram->setUniformValue("textureV", 2);
			}
		}
	} else if (QVideoFrameFormat::Format_BGRA8888 == m_framePixelFormat) {
		if (m_programPixelFormat != m_framePixelFormat && m_shaderProgram) {
			m_shaderProgram.reset();
		}
		if (!m_shaderProgram) {
			m_programPixelFormat = m_framePixelFormat;
			m_shaderProgram.reset(new QOpenGLShaderProgram(this));
			if (m_shaderProgram->create()) {
				if (m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, SHADER_FOR_VERTEX) &&
				    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, SHADER_FOR_BGRA)) {
					m_shaderProgram->link();
				}
			}
		}

		m_shaderProgram->bind();

		QMatrix4x4 mat4;
		if (static_cast<float>(width()) / height() < static_cast<float>(m_glTextureY->width()) / m_glTextureY->height()) {
			auto dstHeight = static_cast<float>(width()) * m_glTextureY->height() / m_glTextureY->width() / height();
			mat4.scale(1, dstHeight);
		} else {
			auto dstWidth = static_cast<float>(height()) * m_glTextureY->width() / m_glTextureY->height() / width();
			mat4.scale(dstWidth, 1);
		}
		m_shaderProgram->setUniformValue("matFullScreen", mat4.transposed());

		m_shaderProgram->enableAttributeArray("position");
		m_shaderProgram->setAttributeBuffer("position", GL_FLOAT, 0, 2, sizeof(float) * 4);
		m_shaderProgram->enableAttributeArray("texcoord");
		m_shaderProgram->setAttributeBuffer("texcoord", GL_FLOAT, sizeof(float) * 2, 2, sizeof(float) * 4);

		if (m_shaderProgram->isLinked()) {
			if (m_glTextureY && m_glTextureY->isCreated()) {
				m_glTextureY->bind(0);
				m_shaderProgram->setUniformValue("textureBGRA", 0);
			}
		}
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PLSMediaRender::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit clicked();
	}
}

void PLSMediaRender::showAIBadge(const QPixmap &pixmap, bool bLongAIBadge)
{
	imageLabel->showAIBadge(pixmap, bLongAIBadge);
}
