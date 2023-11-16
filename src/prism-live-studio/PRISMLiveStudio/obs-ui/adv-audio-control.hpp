#pragma once

#include <obs.hpp>
#include <QWidget>
#include <QPointer>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include "balance-slider.hpp"

class QGridLayout;
class QLabel;
class QSpinBox;
class PLSCheckBox;
class QComboBox;

enum class VolumeType {
	dB,
	Percent,
};

class OBSAdvAudioCtrl : public QObject {
	Q_OBJECT

private:
	OBSSource source;

	QPointer<QWidget> mixerContainer;
	QPointer<QWidget> balanceContainer;
	QPointer<QWidget> monoContainer;
	QPointer<QWidget> nameContainer;

	QPointer<QLabel> iconLabel;
	QPointer<QLabel> nameLabel;
	QPointer<QLabel> active;
	QPointer<QStackedWidget> stackedWidget;
	QPointer<QSpinBox> percent;
	QPointer<QDoubleSpinBox> volume;
	QPointer<PLSCheckBox> forceMono;
	QPointer<BalanceSlider> balance;
	QPointer<QLabel> labelL;
	QPointer<QLabel> labelR;
	QPointer<QSpinBox> syncOffset;
	QPointer<QComboBox> monitoringType;
	QPointer<PLSCheckBox> mixer1;
	QPointer<PLSCheckBox> mixer2;
	QPointer<PLSCheckBox> mixer3;
	QPointer<PLSCheckBox> mixer4;
	QPointer<PLSCheckBox> mixer5;
	QPointer<PLSCheckBox> mixer6;

	OBSSignal volChangedSignal;
	OBSSignal syncOffsetSignal;
	OBSSignal flagsSignal;
	OBSSignal monitoringTypeSignal;
	OBSSignal mixersSignal;
	OBSSignal activateSignal;
	OBSSignal deactivateSignal;
	OBSSignal balChangedSignal;
	OBSSignal renameSignal;

	static void OBSSourceActivated(void *param, calldata_t *calldata);
	static void OBSSourceDeactivated(void *param, calldata_t *calldata);
	static void OBSSourceFlagsChanged(void *param, calldata_t *calldata);
	static void OBSSourceVolumeChanged(void *param, calldata_t *calldata);
	static void OBSSourceSyncChanged(void *param, calldata_t *calldata);
	static void OBSSourceMonitoringTypeChanged(void *param,
						   calldata_t *calldata);
	static void OBSSourceMixersChanged(void *param, calldata_t *calldata);
	static void OBSSourceBalanceChanged(void *param, calldata_t *calldata);
	static void OBSSourceRenamed(void *param, calldata_t *calldata);

public:
	OBSAdvAudioCtrl(QGridLayout *layout, obs_source_t *source_);
	virtual ~OBSAdvAudioCtrl();

	inline obs_source_t *GetSource() const { return source; }
	void ShowAudioControl(QGridLayout *layout);

	void SetVolumeWidget(VolumeType type);
	void SetIconVisible(bool visible);

public slots:
	void SourceActiveChanged(bool active);
	void SourceFlagsChanged(uint32_t flags);
	void SourceVolumeChanged(float volume);
	void SourceSyncChanged(int64_t offset);
	void SourceMonitoringTypeChanged(int type);
	void SourceMixersChanged(uint32_t mixers);
	void SourceBalanceChanged(int balance);
	void SetSourceName(QString newName);

	void volumeChanged(double db);
	void percentChanged(int percent);
	void downmixMonoChanged(bool checked);
	void balanceChanged(int val);
	void syncOffsetChanged(int milliseconds);
	void monitoringTypeChanged(int index);
	void mixer1Changed(bool checked);
	void mixer2Changed(bool checked);
	void mixer3Changed(bool checked);
	void mixer4Changed(bool checked);
	void mixer5Changed(bool checked);
	void mixer6Changed(bool checked);
	void ResetBalance();
};
