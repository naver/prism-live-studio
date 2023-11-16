#pragma once

#include "frontend-api-global.h"
#include "frontend-api.h"
#include <functional>
#include <QJsonObject>
#include <QWidget>

enum class PLSPlatformType { Twitch, YouTube, Facebook, Google, Twitter, Naver, Line, NaverTv, Vlive, Band, AfreecaTV, WhaleSpace, NaverShoppingLive, Email };

class FRONTEND_API PLSLoginInfo {
protected:
	PLSLoginInfo() = default;
	virtual ~PLSLoginInfo() = default;

public:
	enum class UseFor {
		Prism = 0x01,                    // only for prism login
		Channel = 0x02,                  // only for channel login
		Store = 0x04,                    // only for naver shopping live store login
		Prism_Channel = Prism | Channel, // Prism and Channel
		Channel_Store = Channel | Store, // Channel and Store
		All = Prism | Channel | Store,   // all
	};
	enum class ChannelSupport {
		Account, // only support account login
		Rtmp,    // only support rtmp
		Both     // both
	};
	enum class ImplementType { Synchronous, Asynchronous };

	static inline bool isUseFor(UseFor useFor, UseFor checkFor) { return (static_cast<int>(useFor) & static_cast<int>(checkFor)) ? true : false; }

	/**
	  * login module use for
	  * return:
	  *     Prism, Channel, Both
	  */
	virtual UseFor useFor() const = 0;

	/**
	  * login platform
	  * return:
	  *     Twitch, YuTube, Facebook, ...
	  */
	virtual PLSPlatformType platform() const = 0;
	/**
	  * login platform name
	  * return:
	  *     Twitch, YuTube, Facebook, ...
	  */
	virtual QString name() const = 0;
	/**
	  * login platform icon
	  * param:
	  *     [in] useFor: Prism or Channel
	  * return:
	  *     icon full path
	  */
	virtual QString icon(UseFor useFor) const = 0;
	/**
	  * channel login platform support
	  * return:
	  *     Account, Rtmp, Both
	  */
	virtual ChannelSupport channelSupport() const;
	/**
	  * loginWithAccount implement type
	  * return:
	  *     ImplementType
	  */
	virtual ImplementType loginWithAccountImplementType() const;

	/**
	  * login by account way
	  * param:
	  *     [out] result: result data, for example: token, cookies, ...
	  *     [in] useFor: Prism or Channel
	  *     [in-opt] parent: parent widget
	  * return:
	  *     true for success, false for failed
	  */
	virtual bool loginWithAccount(QJsonObject &result, UseFor useFor, QWidget *parent = nullptr) const;
	virtual void loginWithAccountAsync(const std::function<void(bool ok, const QJsonObject &)> &callback, UseFor useFor, QWidget *parent = nullptr) const;

	/**
	  * get rtmp server url address
	  * return:
	  *     rtmp server url address
	  */
	virtual QString rtmpUrl() const;
	/**
	  * rtmp view
	  * param:
	  *     [out] result: result data, for example: token, cookies, ...
	  *     [in] useFor: Prism or Channel
	  *     [in-opt] parent: parent widget
	  * return:
	  *     true for success, false for failed
	  */
	virtual bool rtmpView(QJsonObject &result, UseFor useFor, QWidget *parent = nullptr) const;

	template<typename T> static T *instance()
	{
		static T t;
		return &t;
	}

private:
	Q_DISABLE_COPY(PLSLoginInfo)
};
