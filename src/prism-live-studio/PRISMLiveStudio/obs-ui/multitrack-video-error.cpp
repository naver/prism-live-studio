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
		return PLSAlertView::question(parent, QTStr("Output.StartStreamFailed"),
					      error + QTStr("FailedToStartStream.WarningRetryNonMultitrackVideo")
							      .arg(multitrack_video_name)) == PLSAlertView::Button::Yes;
	} else if (type == Type::Critical) {
		pls_alert_error_message(parent, QTStr("Output.StartStreamFailed"), error);
	}

	return false;
}
