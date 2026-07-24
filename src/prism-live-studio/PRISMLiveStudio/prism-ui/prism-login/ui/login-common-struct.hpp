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
enum class AuthType {
	None,
	Naver,
	Facebook,
	Line,
	Twitch,
	TWitter,
	Google,
	EMail,
};

// third party login type
enum class LoginSupportType {
	NONE = 0,
	LOGIN_TYPE_NAVER,
	LOGIN_TYPE_LINE,
	LOGIN_TYPE_GOOGLE,
	LOGIN_TYPE_TWITTER,
	LOGIN_TYPE_FACEBOOK,
	LOGIN_TYPE_TWITCH,

};

#endif // LOGINCOMMONSTRUCT_H
