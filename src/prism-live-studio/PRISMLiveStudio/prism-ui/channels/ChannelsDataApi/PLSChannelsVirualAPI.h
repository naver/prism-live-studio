#pragma once
#include <QJsonDocument>
#include <QString>
#include <QVariantMap>
#include "PLSErrorHandler.h"
#include "pls-channel-const.h"
class QNetworkReply;
using ReplyPtrs = QSharedPointer<QNetworkReply>;
class LoadingFrame;
using InfosList = QList<QVariantMap>;

/* macro just for develop tip when any function in progress */
#define API_TIP                                                \
	QMessageBox::information(nullptr, QObject::tr("Tips"), \
				 QObject::tr("this function should be done in file %1 \n line %2 \n function %3").arg(QString(__FILE__), QString::number(__LINE__), QString(__FUNCTION__)));

/* API for any to show live info of give json */
void showLiveInfo(const QString &uuid);

void showChannelInfo(const QString &uuid);

/* API for show share view */
void showShareView(const QString &channelUUID);

void addLoginChannel(const QVariantMap &retMap);
bool updateChannelInfoFromNet(const QString &uuid);

void sortInfosByKey(InfosList &infos, const QString &sortKey);

bool handleChannelStatus(const QVariantMap &info);
void handleErrorState(const QVariantMap &info, const QString &uuid);
void handleValidState(const QVariantMap &info, const QString &uuid, bool &isContinue);
void handleLoginErrorState(const QVariantMap &info, const QString &uuid);
void handleEmptyState(const QString &uuid);
void handleWaitingState(const QString &uuid);
void handleUninitialState(const QString &uuid);
bool updateChannelTypeFromNet(const QString &uuid, bool bRefresh = false);
bool updateRTMPTypeFromNet(const QString &uuid);
void showResolutionTips(const QString &platform);

void addErrorFromInfo(const QVariantMap &info);
void addErrorFromRetData(const PLSErrorHandler::RetData &data);
QVariantMap createScheduleGetError(const QString &platform, const PLSErrorHandler::RetData &data);

void refreshChannel(const QString &uuid);

void resetChannel(const QString &uuid);
void resetExpiredChannel(const QString &uuid, bool toAsk = true);
void reloginChannel(const QString &platformName, bool toAsk = true, const PLSErrorHandler::RetData &data = {});

void reloginPrismExpired(const PLSErrorHandler::RetData &data);
//reset channel state when end live ,try check channel state and delete
void resetAllChannels();

void refreshAllChannels();

void handleEmptyChannel(const QString &uuid);

void addRTMP(const QString &channleName = QString());
void editRTMP(const QString &uuid);
void runCMD(const QString &cmdStr);

bool isCurrentVersionCanDoNext(const QStringList &, QWidget *parent);

bool isCurrentVersionCanDoNext(const QString &, QWidget *parent);

bool checkVersion();
bool checkChannelsState();

bool startStreamingCheck();
bool toGoLive();

bool stopStreamingCheck();
bool toStopLive();

bool startRecordCheck();
bool toRecord();

bool stopRecordCheck();
bool toTryStopRecord();

void childExclusive(const QString &channelID);

bool isExclusiveChannel(const QString &uuid);
bool isExclusiveChannel(const QVariantMap &info);
bool isEnabledExclusiveChannelExist();

QString findExistEnabledExclusiveChannel();

void exclusiveChannelCheckAndVerify(const QVariantMap &Newinfo);

void disableAll(bool bSync = false);

/* API for create and  show Busy view */
LoadingFrame *createBusyFrame(QWidget *parent = nullptr);

void showNetworkErrorAlert();
void showAlertOnlyOnce(const PLSErrorHandler::RetData &data);

void showChannelsSetting(int index = 0);

void showChannelsSetting(const QString &platform);

void sendInfoToWidzard(const QVariantMap &info, int type = 0);

void sendLoadingState(bool isBussy = true);

void sendChannelData(const QVariantMap &info);

void handleLauncherMsg(int type, const QJsonObject &data);

void updatePlatformViewerCount();
void activateWindow();
bool hasModalityWindow();
void selectChannel(const QJsonObject &data);

void testError();
