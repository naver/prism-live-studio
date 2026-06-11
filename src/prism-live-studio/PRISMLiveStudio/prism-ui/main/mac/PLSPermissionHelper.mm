//
//  PLSAVFoundataionHeler.mm
//  PRISM Live Studio
//
//  Created by jimbo on 2023/6/16.
//

#import "PLSPermissionHelper.h"
#import <Foundation/Foundation.h>

#import <AVFoundation/AVFoundation.h>
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSAlertView.h"
#import <AppKit/NSWorkspace.h>

using namespace common;

PLSPermissionHelper::AVStatus PLSPermissionHelper::checkPermissionWithSource(obs_source_t *source, AVType &avType)
{
	return checkPermissionWithSource(obs_source_get_id(source), avType);
}

PLSPermissionHelper::AVStatus PLSPermissionHelper::checkPermissionWithSource(const QString &sourceID, AVType &avType)
{

	if (sourceID.isEmpty()) {
		avType = AVType::None;
		return PLSPermissionHelper::AVStatus::Allow;
	}

	if (sourceID.startsWith(OBS_DSHOW_SOURCE_ID)) {
		avType = AVType::Video;
		return getVideoPermissonStatus();
	}
	if (sourceID.startsWith(PRISM_LENS_MOBILE_SOURCE_ID)) {
		avType = AVType::Mobile;
		return getVideoPermissonStatus();
	}
	if (sourceID.startsWith(PRISM_LENS_SOURCE_ID)) {
		avType = AVType::Lens;
		return getVideoPermissonStatus();
	}
	if (sourceID.startsWith(AUDIO_INPUT_SOURCE_ID) || sourceID.startsWith(AUDIO_OUTPUT_SOURCE_ID) || sourceID.startsWith(AUDIO_OUTPUT_SOURCE_ID_V2)) {
		avType = AVType::Audio;
		return getAudioPermissonStatus();
	}
	if (sourceID.startsWith(PRISM_MONITOR_SOURCE_ID) || sourceID.startsWith(WINDOW_SOURCE_ID) || sourceID.startsWith(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)) {
		avType = AVType::Screen;
		return getScreenRecordPermissonStatus();
	}

	if (sourceID.startsWith(OBS_MACOS_AUDIO_CAPTURE_SOURCE_ID)) {
		avType = AVType::ScreenAudio;
		return getScreenRecordPermissonStatus();
	}

	avType = AVType::None;
	return PLSPermissionHelper::AVStatus::Allow;
}

PLSPermissionHelper::AVStatus PLSPermissionHelper::getAudioPermissonStatus(const QObject *receiver, PLSPermissionCallback permissionCallbackProc)
{
	AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
	if (authStatus == AVAuthorizationStatusAuthorized) {
		PLS_INFO(MAC_PERMISSION, "mac permission check audio: Authorized");
		return PLSPermissionHelper::AVStatus::Allow;
	} else if (authStatus == AVAuthorizationStatusDenied) {
		PLS_INFO(MAC_PERMISSION, "mac permission check audio: Denied");
		return PLSPermissionHelper::AVStatus::Denied;
	} else if (authStatus == AVAuthorizationStatusRestricted) {
		PLS_INFO(MAC_PERMISSION, "mac permission check audio: Restricted");
		return PLSPermissionHelper::AVStatus::Restricted;
	} else if (authStatus == AVAuthorizationStatusNotDetermined) {
		PLS_INFO(MAC_PERMISSION, "mac permission check audio: NotDetermined");
		[AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
					 completionHandler:^(BOOL granted) {
						 PLS_INFO(MAC_PERMISSION, "mac permission check audio: granted:%i", granted);
						 if (permissionCallbackProc) {
							 permissionCallbackProc((void *)receiver, (bool)granted);
						 }
					 }];
		return PLSPermissionHelper::AVStatus::NotDetermined;
	}
	PLS_INFO(MAC_PERMISSION, "mac permission check audio: DirectAllow");
	return PLSPermissionHelper::AVStatus::Allow;
}

PLSPermissionHelper::AVStatus PLSPermissionHelper::getVideoPermissonStatus(const QObject *receiver, PLSPermissionCallback permissionCallbackProc)
{
	AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
	if (authStatus == AVAuthorizationStatusAuthorized) {
		PLS_INFO(MAC_PERMISSION, "mac permission check video: Authorized");
		return PLSPermissionHelper::AVStatus::Allow;
	} else if (authStatus == AVAuthorizationStatusDenied) {
		PLS_INFO(MAC_PERMISSION, "mac permission check video: Denied");
		return PLSPermissionHelper::AVStatus::Denied;
	} else if (authStatus == AVAuthorizationStatusRestricted) {
		PLS_INFO(MAC_PERMISSION, "mac permission check video: Restricted");
		return PLSPermissionHelper::AVStatus::Restricted;
	} else if (authStatus == AVAuthorizationStatusNotDetermined) {
		PLS_INFO(MAC_PERMISSION, "mac permission check video: NotDetermined");
		[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
					 completionHandler:^(BOOL granted) {
						 PLS_INFO(MAC_PERMISSION, "mac permission check video: granted:%i", granted);
						 if (permissionCallbackProc) {
							 permissionCallbackProc((void *)receiver, (bool)granted);
						 }
					 }];
		return PLSPermissionHelper::AVStatus::NotDetermined;
	}
	PLS_INFO(MAC_PERMISSION, "mac permission check video: DirectAllow");
	return PLSPermissionHelper::AVStatus::Allow;
}

PLSPermissionHelper::AVStatus PLSPermissionHelper::getScreenRecordPermissonStatus()
{
	bool isAuth = CGPreflightScreenCaptureAccess();
	PLS_INFO(MAC_PERMISSION, "mac permission check screen record: %s", isAuth ? "Authorized" : "Denied");
	return isAuth ? PLSPermissionHelper::AVStatus::Allow : PLSPermissionHelper::AVStatus::Denied;
}

void PLSPermissionHelper::showPermissionAlertIfNeeded(AVType avType, AVStatus helper, QWidget *parent, const std::function<void()> &callBack)
{
	if (helper == AVStatus::Allow || helper == AVStatus::NotDetermined) {
		return;
	}
	if (avType == AVType::None) {
		return;
	}
	NSString *urlString;
	QString alertMsg;
	switch (avType) {
	case AVType::Audio:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone";
		alertMsg = QObject::tr("Mac.Permission.Failed.Audio");
		break;
	case AVType::Video:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_Camera";
		alertMsg = QObject::tr("Mac.Permission.Failed.Video");
		break;
	case AVType::Lens:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_Camera";
		alertMsg = QObject::tr("Mac.Permission.Failed.Lens");
		break;
	case AVType::Mobile:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_Camera";
		alertMsg = QObject::tr("Mac.Permission.Failed.Mobile");
		break;
	case AVType::Screen:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture";
		alertMsg = QObject::tr("Mac.Permission.Failed.Screen");
		break;
	case AVType::ScreenAudio:
		urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture";
		alertMsg = QObject::tr("Mac.Permission.Failed.ScreenAudio");
		break;
	default:
		break;
	}
	PLS_INFO(MAC_PERMISSION, "mac permission show alert with type: %d", avType);

	PLSAlertView alertView(parent, PLSAlertView::Icon::Warning, QObject::tr("Alert.Title"), alertMsg, QString(),
			       QMap<PLSAlertView::Button, QString>({{PLSAlertView::Button::Ok, QObject::tr("Ok")}, {PLSAlertView::Button::Open, QObject::tr("Mac.Permission.Open.System.Btn")}}),
			       PLSAlertView::Button::Open, {{"minBtnWidth", 180}});
	PLSAlertView::Button button = static_cast<PLSAlertView::Button>(alertView.exec());

	if (button == PLSAlertView::Button::Open) {
		PLS_INFO(MAC_PERMISSION, "mac permission show alert and click open button");
		[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]];
	}
	if (callBack) {
		callBack();
	}
}
