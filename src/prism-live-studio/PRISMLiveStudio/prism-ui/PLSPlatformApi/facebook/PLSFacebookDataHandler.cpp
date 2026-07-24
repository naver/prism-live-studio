#include "PLSFacebookDataHandler.h"
#include "PLSChannelDataAPI.h"
#include "../PLSPlatformApi.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
#include "libui.h"
#include <QPointer>

using namespace common;

namespace {

constexpr auto FacebookGetTokenFailed = "Facebook get token failed";
constexpr auto FacebookPlatformReleased = "Facebook platform released";

QVariantMap makeFacebookTokenErrorInfo(const QVariantMap &srcInfo, const QString &reason)
{
	QVariantMap info = srcInfo;
	info[ChannelData::g_channelSreLoginFailed] = reason;
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
	info[ChannelData::g_errorString] = reason;
	return info;
}

void updateFacebookUserInfo(const QPointer<PLSPlatformFacebook> &facebook, const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	if (!facebook) {
		finishedCall(QList<QVariantMap>{makeFacebookTokenErrorInfo(srcInfo, FacebookPlatformReleased)});
		return;
	}
	facebook->setSrcInfo(srcInfo);
	auto userInfoFinished = [finishedCall, facebook, srcInfo](const PLSErrorHandler::RetData &) {
		if (!facebook) {
			finishedCall(QList<QVariantMap>{makeFacebookTokenErrorInfo(srcInfo, FacebookPlatformReleased)});
			return true;
		}
		QVariantMap info = facebook->getSrcInfo();
		finishedCall(QList<QVariantMap>{info});
		return true;
	};
	facebook->getLongLivedUserAccessToken(userInfoFinished);
}

}

QString PLSFacebookDataHandler::getPlatformName()
{
	return FACEBOOK;
}

bool PLSFacebookDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	QPointer<PLSPlatformFacebook> facebook = dynamic_cast<PLSPlatformFacebook *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!facebook) {
		QVariantMap info = srcInfo;
		PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s %s Facebook refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s channelUUID(%s) Facebook platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toUtf8().constData(), facebook.data());
	facebook->setInitData(srcInfo);

	auto updateUserInfo = [facebook, finishedCall](const QVariantMap &info) {
		if (!facebook) {
			finishedCall(QList<QVariantMap>{makeFacebookTokenErrorInfo(info, FacebookPlatformReleased)});
			return;
		}
		QMetaObject::invokeMethod(facebook.data(), [facebook, info, finishedCall]() { updateFacebookUserInfo(facebook, info, finishedCall); });
	};

	if (srcInfo.value(ChannelData::g_channelToken).toString().isEmpty() && !srcInfo.value(ChannelData::g_channelCode).toString().isEmpty()) {
		getShortLivedUserAccessToken(srcInfo, [finishedCall, updateUserInfo](bool ok, const QVariantMap &info) {
			if (!ok) {
				finishedCall(QList<QVariantMap>{info});
				return;
			}
			updateUserInfo(info);
		});
		return true;
	}

	updateUserInfo(srcInfo);
	return true;
}

// PRISM_PC-6311: overrides ChannelDataBaseHandler::loginWithWebPage() (PLSChannelDataHandler.cpp)
// to fix a Facebook-specific duplicate-channel bug: a known race lets a fresh OAuth code arrive
// for an account that already has a connected channel (e.g. user clicks Cancel on Facebook's
// page but the code had already been generated/delivered). The base class always adds a new
// channel with zero duplicate checking; this override reproduces the exact same flow but, when
// an OAuth code is present, resolves the code to a Facebook account id first and updates the
// existing channel instead of creating a duplicate if that account is already connected. Any
// failure along the dedup path (or no match) falls through to the base class's original
// "add new channel" behavior, unchanged.
void PLSFacebookDataHandler::loginWithWebPage(const QString &cmdStr)
{
	this->resetData();
	myLastInfo() = createDefaultChannelInfoMap(getPlatformName(), ChannelData::ChannelType, cmdStr);
	auto retMap = myLastInfo();
	PRE_LOG_MSG_STEP(QString(" show %1 login page ").arg(cmdStr), ChannelData::g_addChannelStep, INFO)
	auto handleCallback = [retMap, cmdStr, this](bool ok, const QVariantHash &result) mutable {
		if (!ok) {
			if (result.value(ChannelData::g_expires_in).toBool()) {
				PLS_INFO("Channels", "%s login failed ,prism token expired", cmdStr.toUtf8().constData());
				auto retData = result.value(ChannelData::g_errorRetdata).value<PLSErrorHandler::RetData>();
				reloginPrismExpired(retData);
			}
			emit this->loginFinished();
			return;
		}

		for (const auto &key : result.keys())
			retMap[key] = result[key];

		const QString code = retMap.value(ChannelData::g_channelCode).toString();
		if (code.isEmpty()) {
			// no OAuth code to dedup against: identical to base class behavior for ok == true.
			PLSCHANNELS_API->addChannelInfo(retMap);
			PLSBasic::instance()->updateMainViewAlwayTop();
			emit this->loginFinished();
			return;
		}

		findExistingFacebookChannelForCode(code, [this, retMap](const QString &existingChannelUUID, const QString &token) {
			// PRISM_PC-6311: findExistingFacebookChannelForCode() guarantees this callback runs
			// on the main/GUI thread (see its implementation below), so it is safe here to touch
			// GUI-thread-only APIs (PLSCHANNELS_API, PLSBasic, tryToUpdate()) directly.
			if (existingChannelUUID.isEmpty()) {
				// no match, or the dedup lookup failed: fall through to the base class's
				// original "add new channel" behavior. findExistingFacebookChannelForCode()
				// already exchanged this OAuth code for a short-lived token in order to resolve
				// the account id for comparison, and Facebook OAuth codes are single-use. If
				// this new channel is added with the now-stale code and no token, the async
				// updateChannelTypeFromNet()/tryToUpdate() chain that addChannelInfo() triggers
				// will try to exchange the already-consumed code again, which fails and leaves
				// the newly added channel in an Error state. Reuse the already-obtained token
				// instead, exactly as the match-found branch below does, so that later
				// tryToUpdate() call skips its own exchange.
				QVariantMap addMap = retMap;
				if (!token.isEmpty()) {
					addMap[ChannelData::g_channelToken] = token;
					addMap.remove(ChannelData::g_channelCode);
				}
				PLSCHANNELS_API->addChannelInfo(addMap);
				PLSBasic::instance()->updateMainViewAlwayTop();
				emit this->loginFinished();
				return;
			}

			// match found: silently update the existing channel instead of adding a duplicate.
			PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s dedup matched channelUUID(%s), silently refreshing the existing channel instead of adding a new one", PrepareInfoPrefix, __FUNCTION__,
				 existingChannelUUID.toUtf8().constData());

			// Seed from the full persisted channel info so this update is a merge, not a
			// replacement (tryToUpdate()/setInitData()/setSrcInfo() replace wholesale, so a
			// sparse map would wipe cached name/cookie/display order/etc).
			QVariantMap updateInfo = PLSCHANNELS_API->getChannelInfo(existingChannelUUID);
			// findExistingFacebookChannelForCode() already exchanged this OAuth code for a
			// short-lived access token in order to resolve the account id for comparison, and
			// Facebook OAuth codes are single-use. Reuse that already-obtained token here instead
			// of forcing tryToUpdate() to exchange the (already consumed) code again: a repeat
			// exchange would almost certainly fail, and that failure path inside
			// tryToUpdate()/getShortLivedUserAccessToken() has no thread re-marshal guarantee, so
			// it could end up invoking finishedCall on a background thread. Setting a non-empty
			// token here makes tryToUpdate() skip its own exchange and go straight to
			// updateUserInfo(). The OAuth code is intentionally omitted: it has already been
			// consumed and is not needed (and not used) once a token is present.
			updateInfo[ChannelData::g_channelToken] = token;
			updateInfo[ChannelData::g_channelUUID] = existingChannelUUID;
			this->tryToUpdate(updateInfo, [this](const InfosList &infos) {
				// Mirrors the other tryToUpdate() call site (updateChannelTypeFromNet() in
				// PLSChannelsVirualAPI.cpp): push the refreshed info back into the channel model
				// so the channel list quietly reflects the updated nickname/avatar/etc. This is
				// not a user-visible notification, just normal channel-list data refresh.
				InfosList tmpList = infos;
				this->updateDisplayInfo(tmpList);
				for (const auto &info : tmpList) {
					PLSCHANNELS_API->setChannelInfos(info);
				}
				PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s dedup silent update finished, %d channel(s) refreshed", PrepareInfoPrefix, __FUNCTION__, static_cast<int>(tmpList.size()));
			});
			emit this->loginFinished();
		});
	};

	QMetaObject::invokeMethod(
		pls_get_main_view(),
		[handleCallback, this]() { //
			pls_channel_login_async(handleCallback, getPlatformName(), pls_get_main_view());
		},
		Qt::QueuedConnection);
}

void PLSFacebookDataHandler::findExistingFacebookChannelForCode(const QString &code, const std::function<void(const QString &existingChannelUUID, const QString &token)> &finishedCall)
{
	QVariantMap srcInfo;
	srcInfo[ChannelData::g_channelCode] = code;
	getShortLivedUserAccessToken(srcInfo, [this, finishedCall](bool ok, const QVariantMap &info) {
		if (!ok) {
			PLS_WARN(MODULE_PLATFORM_FACEBOOK, "%s %s dedup token exchange failed, falling back to add-new-channel", PrepareInfoPrefix, __FUNCTION__);
			// PRISM_PC-6311: getShortLivedUserAccessToken()'s pls::http::Request has no
			// .workInMainThread(), so this callback (and everything below it) can run on the
			// background NetworkAccessManager thread. Re-marshal onto the main/GUI thread before
			// invoking finishedCall, since the caller (loginWithWebPage) touches GUI-thread-only
			// APIs directly from within it. This mirrors the QMetaObject::invokeMethod(facebook.data(), ...)
			// pattern already used by tryToUpdate()'s updateUserInfo lambda in this file.
			QMetaObject::invokeMethod(this, [finishedCall]() { finishedCall(QString(), QString()); });
			return;
		}
		const QString token = info.value(ChannelData::g_channelToken).toString();
		if (token.isEmpty()) {
			PLS_WARN(MODULE_PLATFORM_FACEBOOK, "%s %s dedup token exchange returned an empty token, falling back to add-new-channel", PrepareInfoPrefix, __FUNCTION__);
			QMetaObject::invokeMethod(this, [finishedCall]() { finishedCall(QString(), QString()); });
			return;
		}
		// PRISM_PC-6311: getShortLivedUserAccessToken()'s callback that we're in right now can run
		// on the background NetworkAccessManager thread (see the comment on the !ok branch above).
		// getUserIdByToken() calls PLSAPIFacebook::startRequestApi(), which synchronously mutates
		// the singleton's m_reply map and touches GUI-affiliated pointers (PLS_PLATFORM_FACEBOOK,
		// App()->getMainView()) before it ever reaches the .workInMainThread()-marked completion
		// callback below -- that flag only controls where the HTTP response is delivered, not the
		// thread this initiating call itself runs on. Re-marshal onto the main thread before
		// calling it, same as the two branches above.
		QMetaObject::invokeMethod(this, [this, finishedCall, token]() {
			PLSFaceBookRquest->getUserIdByToken(token, [this, finishedCall, token](bool ok2, const QString &newUserId) {
				if (!ok2 || newUserId.isEmpty()) {
					PLS_WARN(MODULE_PLATFORM_FACEBOOK, "%s %s dedup getUserIdByToken failed or returned an empty id, falling back to add-new-channel", PrepareInfoPrefix,
						 __FUNCTION__);
					// PRISM_PC-6311: the short-lived token above was already successfully obtained
					// (only the userId lookup failed), and the OAuth code that produced it is already
					// consumed. Pass the token through anyway so the add-new-channel fallback can
					// reuse it instead of retrying with the now-stale code. No re-marshal needed: we
					// are already on the main thread, both because we invoked getUserIdByToken() from
					// it above and because its own completion is .workInMainThread()-marked.
					finishedCall(QString(), token);
					return;
				}
				// Already on the main thread (see above), so it's safe to touch GUI-thread-only
				// APIs directly here: PLS_PLATFORM_API::getExistedPlatformsByType(),
				// dynamic_cast<PLSPlatformFacebook *> + getUserId()/getChannelUUID() on the
				// results, and (via finishedCall) the caller's PLSCHANNELS_API/PLSBasic/
				// tryToUpdate() work.
				for (auto *platform : PLS_PLATFORM_API->getExistedPlatformsByType(PLSServiceType::ST_FACEBOOK)) {
					auto *facebook = dynamic_cast<PLSPlatformFacebook *>(platform);
					if (!facebook) {
						continue;
					}
					const QString existingUserId = facebook->getUserId();
					if (existingUserId.isEmpty()) {
						continue;
					}
					if (existingUserId == newUserId) {
						PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s dedup matched existing Facebook channelUUID(%s)", PrepareInfoPrefix, __FUNCTION__,
							 facebook->getChannelUUID().toUtf8().constData());
						finishedCall(facebook->getChannelUUID(), token);
						return;
					}
				}
				PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s dedup found no matching existing Facebook channel, adding new channel", PrepareInfoPrefix, __FUNCTION__);
				// PRISM_PC-6311: this is the genuinely-new-account case. The OAuth code that
				// produced `token` above is already consumed (single-use), so the token must be
				// passed through here as well -- dropping it (calling finishedCall(QString(), QString()))
				// would make the caller fall back to adding the channel with the stale code and no
				// token, and the async add-channel initialization chain's own token exchange would
				// then fail because the code was already used.
				finishedCall(QString(), token);
			});
		});
	});
}

void PLSFacebookDataHandler::getShortLivedUserAccessToken(const QVariantMap &srcInfo, const std::function<void(bool, const QVariantMap &)> &callback)
{
	const QString code = srcInfo.value(ChannelData::g_channelCode).toString();
	if (code.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s, code is empty", FacebookGetTokenFailed);
		callback(false, makeFacebookTokenErrorInfo(srcInfo, FacebookGetTokenFailed));
		return;
	}

	QVariantMap parameters;
	parameters[HTTP_CODE] = code;
	parameters[HTTP_CLIENT_ID] = CHANNEL_FACEBOOK_CLIENT_ID;
	parameters[HTTP_REDIRECT_URI] = FACEBOOK_LOGIN_REDIRECT_URI;
	parameters[HTTP_CLIENT_SECRET] = CHANNEL_FACEBOOK_SECRET;
	parameters[HTTP_GRANT_TYPE] = HTTP_AUTHORIZATION_CODE;

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .url(FACEBOOK_AUTH_TOKEN_URL)
				   .urlParams(parameters)
				   .withLog()
				   .receiver(this)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .objectOkResult([callback, srcInfo](const pls::http::Reply &, const QJsonObject &obj) {
					   const QString accessToken = obj.value(ChannelData::g_channelToken).toString();
					   if (accessToken.isEmpty()) {
						   PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s, access_token is empty", FacebookGetTokenFailed);
						   callback(false, makeFacebookTokenErrorInfo(srcInfo, FacebookGetTokenFailed));
						   return;
					   }

					   QVariantMap info = srcInfo;
					   info[ChannelData::g_channelToken] = accessToken;
					   callback(true, info);
				   })
				   .failResult([callback, srcInfo](const pls::http::Reply &reply) {
					   const QString reason = QString("%1, status code:%2").arg(FacebookGetTokenFailed).arg(reply.statusCode());
					   PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s", reason.toUtf8().constData());
					   callback(false, makeFacebookTokenErrorInfo(srcInfo, reason));
				   }));
}
