#ifndef PLSTAKECAMERASNAPSHOT_H
#define PLSTAKECAMERASNAPSHOT_H

#include <QHBoxLayout>

#include "PLSDialogView.h"
#include "qt-display.hpp"
#include "screenshot-obj.hpp"

namespace Ui {
class PLSTakeCameraSnapshot;
}

class PLSTakeCameraSnapshot;
class PLSLoadingEvent;

#if defined(Q_OS_WINDOWS)
class PLSTakeCameraSnapshotTakeTimerMask : public QDialog {
	Q_OBJECT

public:
	explicit PLSTakeCameraSnapshotTakeTimerMask(QWidget *parent);
	~PLSTakeCameraSnapshotTakeTimerMask() override = default;

signals:
	void timeout();

public:
	void start(int ctime = 3);
	void stop();

protected:
	bool event(QEvent *event) override;

private:
	QLabel *maskLabel;
	int ctime = 3;
};

#else

class PLSTakeCameraSnapshotTakeTimerMask : public QObject {
	Q_OBJECT

public:
	explicit PLSTakeCameraSnapshotTakeTimerMask(QObject *parent = nullptr);
	~PLSTakeCameraSnapshotTakeTimerMask();

signals:
	void timeout();

public:
	void start(obs_display_t *display, int count);
	void stop();

private:
	void start(int count);
	static void draw(void *data, uint32_t baseWidth, uint32_t baseHeight);

private:
	int count;
	obs_display_t *display;
	std::vector<OBSSource> sources;
};

#endif

class PLSFrameCheck : public QObject {
	Q_OBJECT

public:
	explicit PLSFrameCheck(obs_source_t *source, QObject *object = nullptr);
	~PLSFrameCheck();

	bool getResult() { return m_result; }

signals:
	void finished(bool valid);

protected:
	void timerEvent(QTimerEvent *event) override;

private:
	void checkSourceSizeValid(bool needWait);
	int m_timer500ms = 0;
	int m_count = 0;
	obs_source_t *m_source = nullptr;
	bool m_result = false;
};

class PLSTakeCameraSnapshot : public PLSDialogView {
	Q_OBJECT

public:
	PLSTakeCameraSnapshot(QString &camera, QWidget *parent = nullptr);
	~PLSTakeCameraSnapshot() override;

	QString getSnapshot();
	static QList<QPair<QString, QString>> getCameraList();
	void setSourceValid(bool isValid);

private:
	void init(const QString &camera);
	void ClearSource();
	static void drawPreview(void *data, uint32_t cx, uint32_t cy);
	static void onSourceCaptureState(void *data, calldata_t *calldata);
	static void onSourceImageStatus(void *data, calldata_t *calldata);
	void InitCameraList();
	void initCameraListLambda(obs_properties_t *properties);
	void ShowLoading();
	void HideLoading();
	void stopTimer();

private slots:
	void on_cameraList_currentIndexChanged(int index);
	void on_cameraSettingButton_clicked() const;
	void on_okButton_clicked();
	void on_cancelButton_clicked();
	void onTimerEnd();
	void onSourceCaptureState();
	void onSourceImageStatus(bool status);
#if defined(Q_OS_WINDOWS)
	void CheckEnumTimeout();
#endif

	static void updateProperties(void *data, calldata_t *calldata);

protected:
#if defined(Q_OS_WINDOWS)
	bool event(QEvent *event) override;
#endif
	void done(int) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSTakeCameraSnapshot *ui = nullptr;
	QFrame *cameraUninitMask = nullptr;
	QLabel *cameraUninitMaskText = nullptr;
	PLSTakeCameraSnapshotTakeTimerMask *timerMask = nullptr;
	bool hasError = false;
	bool captureImage = false;
	std::atomic_bool sourceValid = false;
	volatile bool inprocess = false;
	OBSSource source;
	QString imageFilePath;
	QString errorMessage;
	QString &camera;
	PLSLoadingEvent *m_pLoadingEvent = nullptr;
	QWidget *m_pWidgetLoadingBG = nullptr;
	QPointer<ScreenshotObj> m_pScreenCapture = nullptr;

	OBSSignal updatePropertiesSignal;
};

#endif // PLSTAKECAMERASNAPSHOT_H
