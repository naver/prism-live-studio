/*
* @file		LiveInfoDialogs.h
* @brief	To show liveinfo dialog by type
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include <QObject>
#include <QMap>

#include "window-basic-main.hpp"

bool isInPreStream();
void setInPreStream(bool bInPreStream);

int pls_exec_live_Info(const QVariantMap &info, QWidget *parent = PLSBasic::Get());

int pls_exec_live_Info_twitch(const QString &which, const QVariantMap &info = QVariantMap(), QWidget *parent = PLSBasic::Get());
int pls_exec_live_Info_youtube(const QString &which, const QVariantMap &info = QVariantMap(), QWidget *parent = PLSBasic::Get());
