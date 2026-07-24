#include "multitrack-video-error.hpp"

#include <QPushButton>
#include "obs-app.hpp"

MultitrackVideoError MultitrackVideoError::critical(QString error)
{
	return {Type::Critical, error};
}

MultitrackVideoError MultitrackVideoError::warning(QString error)
{
	return {Type::Warning, error};
}

MultitrackVideoError MultitrackVideoError::cancel()
{
	return {Type::Cancel, {}};
}

bool MultitrackVideoError::ShowDialog(QWidget *parent, const QString &multitrack_video_name) const
{
	if (type == Type::Warning) {

		PLSErrorHandler::ExtraData extraData;
		extraData.defaultArg = {multitrack_video_name}; 
		auto retData = PLSErrorHandler::getAlertStringByPrismCode(
			PLSErrorHandler::ALERT_MULTITRACKVIDEO_WARNING_MESSAGE, PLSErrKeyAllAlert, "", extraData);
		
		PLSErrorHandler::ExtraData extraData2("MultitrackVideoError_Warning");
		extraData2.pathValueMap["errorMsg"] = error + retData.alertMsg;
		auto retData2 = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_MULTITRACKVIDEO_WARNING,
								      PLSErrKeyAllAlert, {}, extraData2, parent);
		if (retData2.clickedBtn == PLSAlertView::Button::Yes)
			return true;

	} else if (type == Type::Critical) {
		PLSErrorHandler::ExtraData extraData("MultitrackVideoError_Critical");
		extraData.pathValueMap["errorReason"] = error;
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_MULTITRACKVIDEO_ERROR, PLSErrKeyAllAlert,
						      {}, extraData, parent);
	}

	return false;
}
