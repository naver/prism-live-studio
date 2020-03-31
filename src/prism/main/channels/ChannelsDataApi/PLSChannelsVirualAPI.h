#pragma once
#include <QJsonDocument>
#include <QVariantMap>
#include <QString>
class QNetworkReply;
using ReplyPtrs = QSharedPointer<QNetworkReply>;
class LoadingFrame;

/* macro just for develop tip when any function in progress */
#define API_TIP                                                \
	QMessageBox::information(nullptr, QObject::tr("Tips"), \
				 QObject::tr("this function should be done in file %1 \n line %2 \n function %3").arg(QString(__FILE__), QString::number(__LINE__), QString(__FUNCTION__)));

/* API for any to show live info of give json */
void showLiveInfo(const QString &uuid);

void showChannelInfo(const QString &uuid);

/* API for show share view */
void showShareView(const QString &channelUUID);

void showEndView(bool isRecord);
int showSummaryView();
void showChatView(bool isRebackLogin = false, bool isOnlyShow = false, bool isOnlyInit = false);

void addLoginChannel(const QJsonObject &retJson);
bool updateChannelInfoFromNet(const QString &uuid);
bool updateChannelTypeFromNet(const QString &uuid);

void refreshChannel(const QString &uuid);
void resetRTMPState(const QString &uuid);
void resetChannel(const QString &uuid);
void resetExpiredChannel(const QString &uuid);

bool isPrismReplyExpired(ReplyPtrs reply, const QByteArray &data);
void reloginPrismExpired();

void resetAllChannels();

void refreshAllChannels();

void gotuYoutube(const QString &uuid);

void addRTMP();
void editRTMP(const QString &uuid);
void runCMD(const QString &cmdStr);

bool checkChannelsState();

bool startStreamingCheck();
bool toGoLive();

bool stopStreamingCheck();
bool toStopLive();

bool startRecordCheck();
bool toRecord();

bool stopRecordCheck();
bool stopRecord();

bool isNowChannel(const QString &uuid);
bool isNowChannel(const QVariantMap &info);
bool isEnabledNowExist();
QString findExistEnabledNow();

void nowCheckAndVerify(const QVariantMap &Newinfo);

void disableAll();

/* API for create and  show Busy view */
LoadingFrame *createBusyFrame(QWidget *parent = nullptr);
long getMaxRTMPcount();

void showNetworkErrorAlert();
