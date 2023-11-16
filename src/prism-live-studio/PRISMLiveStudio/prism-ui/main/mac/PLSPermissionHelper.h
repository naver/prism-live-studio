//
//  PLSAVFoundataionHeler.h
//  PRISM Live Studio
//
//  Created by jimbo on 2023/6/16.
//

#pragma once
#include <QString>
#include <obs.hpp>
#include <QObject>

#if defined(Q_OS_MACOS)

typedef void (*PLSPermissionCallback)(void *inUserData, bool isUserClickOK);

struct PLSPermissionHelper {
	enum class AVStatus { Allow, NotDetermined, Denied, Restricted };
	enum class AVType { None, Audio, Video, Screen };

	static AVStatus checkPermissionWithSource(obs_source_t *source, AVType &avType);
	static AVStatus checkPermissionWithSource(const QString &sourceID, AVType &avType);
	static AVStatus getAudioPermissonStatus(const QObject *receiver = nullptr, PLSPermissionCallback permissionCallbackProc = nullptr);
	static AVStatus getVideoPermissonStatus(const QObject *receiver = nullptr, PLSPermissionCallback permissionCallbackProc = nullptr);
	static AVStatus getScreenRecordPermissonStatus();

	static void showPermissionAlertIfNeeded(AVType avType, AVStatus helper, QWidget *parent = nullptr, const std::function<void()> &callBack = nullptr);
};

#endif
