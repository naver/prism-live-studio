#ifndef PLSTHUMBNAILLABEL_H
#define PLSTHUMBNAILLABEL_H

#include "PLSHttpApi/PLSFileDownloader.h"
#include "pls-common-define.hpp"
#include "loading-event.hpp"
#include "PLSGipyStickerView.h"
#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSvgRenderer>

#define LOADING_TIME_MS 200

class PLSThumbnailLabel : public QPushButton {
	Q_OBJECT
	Q_PROPERTY(QSize loadingIconSize READ loadingIconSize WRITE setLoadingIconSize)
	Q_PROPERTY(qreal radio READ radio WRITE setRadio)
public:
	void SetTimer(QTimer *timer)
	{
		if (timer && !m_connectedTimer) {
			if (!timer->isActive())
				timer->start();
			connect(timer, &QTimer::timeout, this, [=]() {
				if (!showLoading)
					return;
				loadingIndex = (loadingIndex + 1 > 8) ? 1 : ++loadingIndex;
				repaint(m_loadingRect);
			});
		}
	}

	QSize loadingIconSize() const { return iconSize; }

	void setLoadingIconSize(const QSize &size)
	{
		iconSize = size;
		m_loadingRect.setX((width() - size.width()) / 2);
		m_loadingRect.setY((height() - size.height()) / 2);
		m_loadingRect.setSize(size);
		repaint(m_loadingRect);
	}

	qreal radio() const { return m_radio; };
	void setRadio(qreal radio)
	{
		m_radio = radio;
		UpdateRect();
	}

	explicit PLSThumbnailLabel(QWidget *parent = nullptr) : QPushButton(parent), m_radio(1.0)
	{
		QHBoxLayout *layoutInner = new QHBoxLayout(this);
		layoutInner->setContentsMargins(0, 0, 0, 0);
		labelThumbnail = new QLabel(this);
		labelThumbnail->setObjectName("thunbnailLabelBorder");
		layoutInner->addWidget(labelThumbnail);
	}
	~PLSThumbnailLabel() {}

	QByteArray GetUniqueFileName(const QUrl &url)
	{
		auto uniqueId = QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
		return uniqueId;
	}

	QString GetCacheFileName(const QUrl &url) { return QString(m_cachePath + GetUniqueFileName(url) + ".cache"); }

	bool CheckNeedRedownload(const QUrl &url, qint64 versionNew)
	{
		auto cacheFileName = GetCacheFileName(url);
		QFile file(cacheFileName);
		if (!file.exists())
			return true;

		QDataStream outData(&file);
		qint64 version = 0LL;
		QByteArray image;
		if (file.open(QIODevice::ReadOnly)) {
			outData >> version;
			outData >> image;
			file.close();
		}

		if (version < versionNew)
			return true;

		return false;
	}

	bool SaveCache(const QByteArray &data, qint64 version)
	{
		auto outFileName = GetCacheFileName(m_url);

		QDir dir = QFileInfo(outFileName).dir();
		if (!dir.exists()) {
			dir.mkpath(dir.absolutePath());
		}

		QFile out(outFileName);
		QDataStream outData(&out);
		if (out.open(QIODevice::WriteOnly)) {
			outData << version;
			outData << data;
			out.close();
			return true;
		}
		return false;
	}

	bool DeleteCache()
	{
		auto cacheFileName = GetCacheFileName(m_url);
		QFile file(cacheFileName);
		return file.remove();
	}

	bool GetImageFromCache()
	{
		auto cache = GetCacheFileName(m_url);
		QFile fileCache(cache);
		QDataStream data(&fileCache);
		if (fileCache.open(QIODevice::ReadOnly)) {
			QFileInfo info(m_url.path());
			qint64 version;
			QByteArray image;
			data >> version;
			data >> image;
			QImage img;
			img.loadFromData(image, qUtf8Printable(info.suffix()));
			m_pixmap = QPixmap::fromImage(img);
			fileCache.close();
			return true;
		}
		return false;
	}

	void SetFileName(const QString &fileName, bool isWebResource = false, qint64 version = 0LL)
	{
		if (isWebResource) {
			if (!CheckNeedRedownload(m_url, version)) {
				GetImageFromCache();
			} else {
				DownloadTaskData task;
				task.url = m_url.toString();
				task.needRetry = true;
				task.outputPath = m_cachePath;
				task.version = m_version;
				QPointer<PLSThumbnailLabel> guarded(this);
				task.rawDataCallback = [guarded](const QByteArray &data, const TaskResponData &result) {
					if (!guarded || result.resultType == ResultStatus::ERROR_OCCUR)
						return;
					QMetaObject::invokeMethod(guarded, "RefreshDownload", Qt::QueuedConnection, Q_ARG(const QByteArray &, data));
				};
				task.randomId = PLSGipyStickerView::ConvertPointer(this);
				PLSFileDownloader::instance()->Get(task, true);
				setProperty("useDefault", true);
				pls_flush_style(this);
				return;
			}
		} else {
			m_pixmap = QPixmap(fileName);
		}
		setProperty("useDefault", false);
		pls_flush_style(this);
		UpdateRect();
		repaint();
	}

	void SetUrl(const QUrl &url, qint64 version = 0LL)
	{
		m_url = url;
		m_version = version;
		if (url.isLocalFile()) {
			SetFileName(url.toLocalFile());
		} else {
			auto fileName = m_cachePath + url.fileName();
			SetFileName(fileName, true, version);
		}
	}

	void SetCachePath(const QString &path) { m_cachePath = path; }

	void SetShowLoad(bool visible)
	{
		showLoading = visible;
		loadingIndex = 1;
		repaint();
	}

	void SetShowOutline(bool visible)
	{
		labelThumbnail->setProperty("select", visible);
		pls_flush_style(labelThumbnail);
	}

	bool IsShowOutline()
	{
		bool ok = labelThumbnail->property("select").toBool();
		return ok;
	}

	bool IsShowLoading() { return showLoading; }

protected:
	void UpdateRect()
	{
		if (m_pixmap.isNull())
			return;
		int pixWidth = m_pixmap.width();
		int pixHeight = m_pixmap.height();
		float radio = 1.0;
		radio = (m_radio * width()) / (pixWidth > pixHeight ? (float)pixWidth : (float)pixHeight);
		int fitWidth = (int)(radio * pixWidth);
		int fitHeight = (int)(radio * pixHeight);
		m_rect.setX((width() - fitWidth) / 2);
		m_rect.setY((height() - fitHeight) / 2);
		m_rect.setWidth(fitWidth);
		m_rect.setHeight(fitHeight);
	}

	void paintEvent(QPaintEvent *event) override
	{
		QPainter dc(this);
		dc.setRenderHint(QPainter::SmoothPixmapTransform);
		if (m_pixmap.isNull())
			goto endofpaint;
		dc.drawPixmap(m_rect, m_pixmap);
	endofpaint:
		if (showLoading) {
			render.load(QString::asprintf(":/images/loading-%d.svg", loadingIndex));
			render.render(&dc, m_loadingRect);
		}
		__super::paintEvent(event);
	}
	void resizeEvent(QResizeEvent *event) override
	{
		if (m_pixmap.isNull()) {
			__super::resizeEvent(event);
			return;
		}
		__super::resizeEvent(event);
		QTimer::singleShot(0, this, [=]() { UpdateRect(); });
	}

	void showEvent(QShowEvent *event) override
	{
		__super::showEvent(event);
		QTimer::singleShot(0, this, [=]() { UpdateRect(); });
	}

private slots:
	void RefreshDownload(const QByteArray &data)
	{ // Embedded version info into file.
		DeleteCache();
		SaveCache(data, m_version);
		SetUrl(m_url, m_version);
	}

private:
	QPixmap m_pixmap;
	QRect m_rect;
	QRect m_loadingRect;
	QLabel *labelThumbnail = nullptr;
	QSvgRenderer render;
	bool showLoading = false;
	bool m_connectedTimer = false;
	int loadingIndex = 1;
	QSize iconSize;
	qreal m_radio;
	QString m_cachePath;
	QUrl m_url;
	qint64 m_version;
};

#endif // PLSTHUMBNAILLABEL_H
