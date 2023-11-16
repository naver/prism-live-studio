#pragma once
#include <obs.h>
#include <obs.hpp>
#include <optional>
#include <vector>
#include <qobject.h>

class PLSAudioControl : public QObject {
	Q_OBJECT
public:
	struct AudioStatus {
		int audioSourceCount = 0; //audio device or audio source
		bool isAllAudioMuted = true;
	};

	static PLSAudioControl *instance();
	~PLSAudioControl() final = default;

	void MuteAll();
	void UnmuteAll();
	void SetMuteState(bool mute);
	bool GetMuteState() const;
	void TriggerMute();
	void ClearObsSignals();

	void InitControlStatus(obs_data_t *data, bool hasValue);
	void InitSourceAudioStatus(obs_source_t *source) const;
	bool CheckMasterAudioStatus(obs_data_t *data);
	void UpdateMasterSwitch(bool mute);
	bool ConfigAllMuted() const;

	void OnSourceItemVisibleChanged(OBSSceneItem item, bool visible);
	void OnSourceItemRemoved(OBSSceneItem item);
	void OnSceneRemoved(OBSSource source);
	void DeactivateAudioSource(OBSSource source);
	void ActivateAudioSource(OBSSource source) const;

private slots:
	void VolumeMuted(const QString &sourceName, bool muted);

private:
	PLSAudioControl();
	void InitObsSignals();
	void UpdateSourceInMixer(const obs_source_t *source) const;

	AudioStatus GetMasterAudioStatusCurrent(const obs_source_t *ignoreAudioSource = nullptr, const obs_source_t *ignoreDeviceSource = nullptr) const;
	AudioStatus GetSourceAudioStatusCurrent(const obs_source_t *ignoreSource) const;
	AudioStatus GetDeviceAudioStatusCurrent(const obs_source_t *ignoreDevice) const;
	AudioStatus GetDeviceAudioStatusPrevious() const;
	AudioStatus GetSourceAudioStatusPrevious() const;
	AudioStatus GetMasterAudioStatusPrevious() const;

	static void SourceCreated(void *data, calldata_t *params);
	static void SourceRemoved(void *data, calldata_t *params);
	static void PLSVolumeMuted(void *data, calldata_t *calldata);

	std::optional<bool> muteAll;
	std::vector<OBSSignal> signalHandlers;
};