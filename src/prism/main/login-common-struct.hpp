
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


#endif // LOGINCOMMONSTRUCT_H
