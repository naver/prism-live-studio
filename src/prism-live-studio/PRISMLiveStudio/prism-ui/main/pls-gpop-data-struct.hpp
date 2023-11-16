#pragma once

#include <QMap>
#include <QVector>
#include <QString>
#define name2str(name) (#name)

using RtmpDestination = struct RtmpDestination {
	QString streamName;
	QString rtmpUrl;
	bool exposure;
	int order;
};

using Common = struct Common {
	QString stage;
	QString regionCode;
	QString deviceType;
};
using channelAttr = struct channelAttr {
	QString id;
	uint order;
};

using ChannelAttrs = struct ChannelAttrs {
	QVector<channelAttr> attrs;
};

using GPopChannelDataList = struct GPopChannelDataList {
	QMap<QString, ChannelAttrs> channels;
};
using CategoryS = struct CategoryS {
	uint id;
	QString nameKr;
	QString nameEn;
};
using eautyFilterPreset = struct BeautyFilterPreset {
	QString id;
	uint order;
	uint version;
	QString url;
};

using YoutubeCategories = struct YoutubeCategories {
	QVector<CategoryS> categories;
};

using RequestPolicy = struct RequestPolicy {
	QString requestType;
	uint timeoutMS;
};
using StartPolicy = struct StartPolicy {
	QString startType;
	uint waitDurationMS;
	uint retryCount;
};
using StatisticPolicy = struct StatisticPolicy {
	QString statisticType;
	uint reqIntervalMS;
};
using AbpPolicy = struct AbpPolicy {
	QString abpPolicyType;
	uint triggerToastMS;
	uint abpExecuteIntervalMS;
	uint bitrateDownThresholdMS;
	uint audioBufferDropThresholdMS;
	float targetBitrateWeight;
	uint videoBitrateStep;
	uint minVideoBitrate;
	uint maxVideoBitrate;
	uint initialVideoBitrate;
	uint initialAudioBitrate;
	uint minBitrateUpThreshold;
	uint maxBitrateUpThreshold;
	float bitrateUpThresholdMultiplier;
	uint bitrateUpDurationThresholdMS;
	uint bitrateUpContinueThresholdMS;
	uint bwStableThresholdMS;
	uint tuningBitrateStepRange;
	float tuningThresholdWeight;
	uint movingAverageSize;
	uint forcedTerminationMS;
};

using Publish = struct Publish {
	QString publishType;
	QString netPolicy;
	QMap<QString, AbpPolicy> abpPolicys;
};
using SnsCallbackUrl = struct SnsCallbackUrl {
	QString callbackUrlType;
	QString url;
};

using Policy = struct Policy {
	bool isN1HCExposure;
	QStringList gameCaptureBlackList;
	QVector<uint> camFpsList;
	bool isExposeCoachmark;
	QMap<QString, RequestPolicy> requests;
	QMap<QString, StartPolicy> statrPolicys;
	QMap<QString, StatisticPolicy> statistics;
	QMap<QString, StatisticPolicy> chats;
	QMap<QString, Publish> publishs;
	QMap<QString, SnsCallbackUrl> snscallbackurls;
};
using InitialData = struct InitialData {
	QVector<GPopChannelDataList> channelLists;
	QVector<BeautyFilterPreset> beautyFilterPresets;
	YoutubeCategories youtube;
};
using Pc = struct Pc {
	QString logLevel;
	InitialData initialData;
	Policy policy;
};

using Optional = struct Optional {
	Common common;
	Pc pc;
};
using Fallback = struct Fallback {
	QString url;
	QString ssl;
};
using Connection = struct Connection {
	QString url;
	QString ssl;
	Fallback fallback;
};
using GpopInfo = struct GPopInfo {
	Connection connection;
	Optional optional;
	long long buildData;
};
using BlackList = struct BlackList {
	QVector<QString> gpuModels;
	QVector<QString> graphicsDrivers;
	QVector<QString> deviceDrivers;
	QVector<QString> thirdPartyPlugins;
	QVector<QString> vstPlugins;
	QVector<QString> thirdPartyPrograms;
	QMap<QString, QString> exceptionTypes;
	bool isEmpty = true;
};

using PlatformLiveTime = struct PlatformLiveTime {
	QStringList countdownReminderMinutesList;
	int maxLiveMinutes;
};

using ResolutionGuide = QVariantMap;
