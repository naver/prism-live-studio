/*
 * @fine      LoginCommonStruct
 * @brief     login common emnu and struct
 * @date      2019-10-10
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGINCOMMONSTRUCT_H
#define LOGINCOMMONSTRUCT_H

#include <QVariant>
#include <QString>
// login AuthType
typedef enum class AuthType {
	None,
	Naver,
	Facebook,
	Line,
	Twitch,
	TWitter,
	Google,
	EMail,
} AuthType;

// third party login type
typedef enum class LoginSupportType {
	NONE = 0,
	LOGIN_TYPE_NAVER,
	LOGIN_TYPE_LINE,
	LOGIN_TYPE_GOOGLE,
	LOGIN_TYPE_TWITTER,
	LOGIN_TYPE_FACEBOOK,
	LOGIN_TYPE_TWITCH,

} LoginSupportType;

// prism login user information
typedef struct UserInfo {
	QVariant nickName;
	QVariant userCode;
	QVariant email;
	QVariant profileThumbnailUrl;
	QVariant authType;
	QVariant token;
	QVariant authStatusCode;
	QVariant hashUserCode;
	void clear()
	{
		nickName.clear();
		userCode.clear();
		email.clear();
		profileThumbnailUrl.clear();
		authType.clear();
		token.clear();
		authStatusCode.clear();
		hashUserCode.clear();
	}
} UserInfo;

#endif // LOGINCOMMONSTRUCT_H
