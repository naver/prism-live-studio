#ifndef PRISMSTICKERSOURCE_HPP
#define PRISMSTICKERSOURCE_HPP

#include <obs-module.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <vector>
#include <QString>
#include <QPair>
#include <QThread>
#include <QObject>

enum class MusicStatus { Noraml, Started, Paused, Resume, Stoped };
enum class TemplateType { One = 0, Two, Three, Four, Five, Six };
enum class TimerType { Current = 0, Live, Countdown, Stopwatch };

struct obs_source_audio;

struct PLSColorData {
	struct color_data {
		const char *name{};
		const char *path{};
		const char *colorValue;
	};

	static PLSColorData *instance();

	void initColorDatas();

	std::vector<color_data> m_vecColors_t23{};
	std::vector<color_data> m_vecColors_t45{};
	std::vector<color_data> m_vecColors_t6{};
	std::vector<QPair<const char *, const char *>> m_vecMusicPath{};
};

struct timer_data {
	/* source and settings don't need call release method */
	obs_source_t *source{};
	obs_data_t *settings{};
	gs_texture_t *web_source_tex = nullptr;
	obs_source_t *web = nullptr;
	obs_source_t *sub_music = nullptr;

	obs_hotkey_id start_hotkey;
	obs_hotkey_id cancel_hotkey;
	MusicStatus mStaus = MusicStatus::Noraml;
};

class AudioConvertThread : public QThread {
public:
	AudioConvertThread();

	void startConvertAudio(obs_source_t *source, const obs_source_audio &audio);

protected:
	void run() override;
};

class timer_source : public QObject {
public:
	timer_data config;
	timer_source(obs_source_t *source, obs_data_t *settings);
	~timer_source() override;

	void timer_source_destroy();
	void timer_source_update();
	void dispatahNoramlJsToWeb();
	void dispatahControlJsToWeb();
	QByteArray toJsonStr();
	void getColorData(QString &type, QString &forColor, QString &bgColor, bool &hasBackground);
	void getTextData(QString &mold, QString &_text);

	void init_music_source();
	void onlyStopMusicIfNeeded();
	void webDidReceiveMessage(const char *msg);

	void updateControlButtons(MusicStatus status, obs_properties_t *props, bool isForce = false, bool isNeedUpdate = true);
	MusicStatus getStatus(const QString &name, bool &isGot) const;
	QString getControlEvent() const;

	void propertiesEditStart();
	void propertiesEditEnd(bool isSaveClick);
	void getControlButtonTextAndEnable(int idx, QString &text, bool &enable);
	bool getStartButtonHightlight() const;

	void updateOKButtonEnable();

	void resetCountdownIfNeed(obs_data_t *settings);

	void updateWebAudioShow(bool isShow);
	long long getCountTime();
	long long getCountTime(TemplateType type);

	bool getListenButtonEnable();
	const char *getColorTitle();

	void didClickDefaultsButton();

	void resetTextToDefaultWhenEmpty();

	void sendAnalogWhenClickSavedBtn();

	bool isChangeControlByHand = false;
	bool isPropertiesOpened = false;
	AudioConvertThread *audioThread{};
};

#endif // PRISMSTICKERSOURCE_HPP
