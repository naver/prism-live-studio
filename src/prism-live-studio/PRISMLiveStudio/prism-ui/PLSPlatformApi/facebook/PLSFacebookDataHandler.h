#ifndef PLSFACEBOOKDATAHANDLER_H
#define PLSFACEBOOKDATAHANDLER_H

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSFacebookDataHandler : public ChannelDataBaseHandler {
public:
	PLSFacebookDataHandler() = default;
	~PLSFacebookDataHandler() override = default;
	QString getPlatformName() override;
	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;
	// PRISM_PC-6311: overrides the base class login flow to dedup against an already-connected
	// Facebook account before adding a brand-new channel (see .cpp for details).
	void loginWithWebPage(const QString &cmdStr) override;

private:
	void getShortLivedUserAccessToken(const QVariantMap &srcInfo, const std::function<void(bool, const QVariantMap &)> &callback);
	// PRISM_PC-6311: resolves an OAuth code to an already-connected Facebook channel's UUID.
	// Calls finishedCall with an empty UUID (and empty token) if there is no match or if any
	// step (token exchange, user id lookup) fails, so the caller can fall back to adding a new
	// channel. On a match, token is the short-lived access token that was already exchanged for
	// the (now consumed) OAuth code while resolving the account id here; the caller must reuse
	// this token (e.g. seed it into tryToUpdate()'s srcInfo) instead of trying to exchange the
	// same code again, since Facebook OAuth codes are single-use. finishedCall is always invoked
	// on the main/GUI thread (see the .cpp for why), so callers may safely touch GUI-thread-only
	// APIs directly from within it.
	void findExistingFacebookChannelForCode(const QString &code, const std::function<void(const QString &existingChannelUUID, const QString &token)> &finishedCall);
};

#endif // PLSFACEBOOKDATAHANDLER_H
