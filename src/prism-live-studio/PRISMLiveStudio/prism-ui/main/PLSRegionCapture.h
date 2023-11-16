#ifndef PLSREGIONCAPTURE_H
#define PLSREGIONCAPTURE_H

#include <qobject.h>
#include <QProcess>
#include <QRect>

class PLSRegionCapture : public QObject {
	Q_OBJECT

public:
	explicit PLSRegionCapture(QObject *parent = nullptr);
	~PLSRegionCapture() noexcept final;
	void StartCapture(uint64_t maxRegionWidth = 0, uint64_t maxRegionHeight = 0);
	QRect GetSelectedRect() const;

signals:
	void selectedRegion(const QRect &rect);

private slots:
	void onCaptureFinished();

private:
	QProcess *m_process = nullptr;
	QRect m_rectSelected;
};

#endif // PLSREGIONCAPTURE_H
