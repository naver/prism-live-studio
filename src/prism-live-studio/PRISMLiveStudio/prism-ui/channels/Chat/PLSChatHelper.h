#pragma once
#include <QObject>
#include <QVariantMap>
#include <vector>

extern const char *const s_ChatStatusType;
extern const char *const s_ChatStatusNormal;
extern const char *const s_ChatStatusOnlyOne;
extern const char *const s_ChatStatusSelect;

struct ChatPlatformIndex {
	const static int All = 0;
	const static int RTMP = 1;
	const static int Twitch = 2;
	const static int Youtube = 3;
	const static int Facebook = 4;
	const static int Chzzk = 5;
	const static int NaverTV = 6;
	const static int VLive = 7;
	const static int AfreecaTV = 8;
	const static int NaverShopping = 9;
	const static int NCB2B = 10;
	const static int UnDefine = 11;
};

class PLSChatHelper : public QObject {
	Q_OBJECT

public:
	enum class ChatFontScale {
		Normal = 0,
		PlusDisable,
		MinusDisable,
	};

	static PLSChatHelper *instance();
	~PLSChatHelper() final;

	bool isLocalHtmlPage(int index) const;
	bool isRemoteHtmlPage(int index) const;
	//toLowerSpace: change to lower and remove white space
	QString getString(int index, bool toLowerSpace = false) const;
	int getIndexFromInfo(const QVariantMap &info) const;
	//only for channel select info
	void getSelectInfoFromIndex(int index, QVariantMap &getInfo) const;
	QString getPlatformNameFromIndex(int index) const;
	QString getRtmpPlaceholderString() const;
	bool isCefWidgetIndex(int index) const;
	bool canChatYoutube(const QVariantMap &info, bool checkForKids) const;
	std::string getChatUrlWithIndex(int index, const QVariantMap &info) const;
	bool showToastWhenStart(QString &showStr) const;
	QString getToastString() const;

	void startToNotify();
	QString getTabButtonCss(const QString &objectName, const QString &platName, const QString &platNameNoLower) const;
	void sendWebShownEventIfNeeded(int index) const;

	PLSChatHelper::ChatFontScale getFontBtnStatus(int scaleSize);
	int getNextScaleSize(bool isToPlus);
	static int getFontScaleSize();
	static void sendWebChatFontSizeChanged(int scaleSize);

	static QString getDispatchJS(int index, const QString &url);
	static QString getYoutubeDisableBackupJS();

private:
	std::vector<int> m_fontScales = {75, 85, 100, 115, 125, 150, 175, 200, 250, 300, 400, 500};
};

#define PLS_CHAT_HELPER PLSChatHelper::instance()
