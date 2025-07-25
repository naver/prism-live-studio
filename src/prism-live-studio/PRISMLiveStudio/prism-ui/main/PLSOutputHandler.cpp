#include "PLSOutputHandler.hpp"

#include <qmetaobject.h>
#include <pls/pls-dual-output.h>

#include "window-basic-main.hpp"
#include "PLSPlatformApi.h"

PLSOutputHandler::PLSOutputHandler() {}

PLSOutputHandler::~PLSOutputHandler() {}

PLSOutputHandler::operator bool() const
{
	if (!houtput) {
		return false;
	} else if (!pls_is_dual_output_on()) {
		return true;
	} else if (!voutput) {
		return false;
	} else {
		return true;
	}
}

bool PLSOutputHandler::operator==(std::nullptr_t) const
{
	return !operator bool();
}

BasicOutputHandler *PLSOutputHandler::operator->()
{
	return houtput.get();
}

const BasicOutputHandler *PLSOutputHandler::operator->() const
{
	return houtput.get();
}

void PLSOutputHandler::reset(OBSBasic *main_)
{
	main = main_;
	houtput.reset();
	voutput.reset();
}

void PLSOutputHandler::reset(bool advOut, OBSBasic *main)
{
	reset(main);
	houtput.reset(advOut ? CreateAdvancedOutputHandler(main, this, false) : CreateSimpleOutputHandler(main, this, false));
	if (pls_is_dual_output_on()) {
		voutput.reset(advOut ? CreateAdvancedOutputHandler(main, this, true) : CreateSimpleOutputHandler(main, this, true));
	}
}

void PLSOutputHandler::resetState()
{
	startStreaming[Horizontal] = startStreaming[Vertical] = false;
	streamDelayStarting[Horizontal] = streamDelayStarting[Vertical] = false;
	streamDelayStopping[Horizontal] = streamDelayStopping[Vertical] = false;
	streamingStartInvoked = false;
	streamingStart[Horizontal] = streamingStart[Vertical] = false;
	streamStopping[Horizontal] = streamStopping[Vertical] = false;
	streamingStop[Horizontal] = streamingStop[Vertical] = false;
	streamDelayStartingSec[Horizontal] = streamDelayStartingSec[Vertical] = 0;
	streamDelayStoppingSec[Horizontal] = streamDelayStoppingSec[Vertical] = 0;
	streamingStopErrorCode[Horizontal] = streamingStopErrorCode[Vertical] = 0;
	streamingStopLastError[Horizontal].clear();
	streamingStopLastError[Vertical].clear();
}

std::pair<std::shared_future<void>, std::shared_future<void>> PLSOutputHandler::SetupStreaming(obs_service_t *service, SetupStreamingContinuation_t continuation, obs_service_t *vservice,
											       SetupStreamingContinuation_t vcontinuation)
{
	if (nullptr == service) {
		return {std::shared_future<void>(), voutput->SetupStreaming(vservice, vcontinuation)};
	} else if (nullptr == vservice) {
		return {houtput->SetupStreaming(service, continuation), std::shared_future<void>()};
	} else {
		return {houtput->SetupStreaming(service, continuation), voutput->SetupStreaming(vservice, vcontinuation)};
	}
}

bool PLSOutputHandler::StartStreaming(obs_service_t *service, obs_service_t *vservice)
{
	resetState();

	if (nullptr == service) {
		startStreaming[Vertical] = voutput->StartStreaming(vservice);
	} else if (nullptr == vservice) {
		startStreaming[Horizontal] = houtput->StartStreaming(service);
	} else {
		startStreaming[Vertical] = voutput->StartStreaming(vservice);
		startStreaming[Horizontal] = houtput->StartStreaming(service);

		if (startStreaming[Horizontal] != startStreaming[Vertical]) {
			auto getMessage = [](BasicOutputHandler *outputHandler) {
				return !outputHandler->lastError.empty() ? QTStr(outputHandler->lastError.c_str())
#ifdef _WIN32
									 : QTStr("Output.StartFailedGeneric");
#else
									 : QTStr("Output.StartFailedGeneric.Mac");
#endif
			};
			if (!startStreaming[Horizontal]) {
				PLS_PLATFORM_API->addFailedPlatform(PLS_PLATFORM_HORIZONTAL);
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(PLSPlatformApi::joinPlatformNames(PLS_PLATFORM_HORIZONTAL), getMessage(houtput.get())));
			} else if (!startStreaming[Vertical]) {
				PLS_PLATFORM_API->addFailedPlatform(PLS_PLATFORM_VERTICAL);
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(PLSPlatformApi::joinPlatformNames(PLS_PLATFORM_VERTICAL), getMessage(voutput.get())));
			}
		}
	}

	return startStreaming[Horizontal] || startStreaming[Vertical];
}

bool PLSOutputHandler::StartRecording()
{
	return houtput->StartRecording();
}

bool PLSOutputHandler::StartReplayBuffer()
{
	return houtput->StartReplayBuffer();
}

bool PLSOutputHandler::StartVirtualCam()
{
	return houtput->StartVirtualCam();
}

void PLSOutputHandler::StopStreaming(bool force, StreamingType streamType)
{
	if (StreamingType::Horizontal == streamType || StreamingType::StreamingTypeMax == streamType) {
		houtput->StopStreaming(force);
	}
	if (pls_is_dual_output_on()) {
		if (StreamingType::Vertical == streamType || StreamingType::StreamingTypeMax == streamType) {
			voutput->StopStreaming(force);
		}
	}
}

void PLSOutputHandler::StopRecording(bool force)
{
	houtput->StopRecording(force);
}

void PLSOutputHandler::StopReplayBuffer(bool force)
{
	houtput->StopReplayBuffer(force);
}

void PLSOutputHandler::StopVirtualCam()
{
	houtput->StopVirtualCam();
}

bool PLSOutputHandler::StreamingActive() const
{
	return houtput->StreamingActive() || (voutput && voutput->StreamingActive());
}

bool PLSOutputHandler::RecordingActive() const
{
	return houtput->RecordingActive();
}

bool PLSOutputHandler::ReplayBufferActive() const
{
	return houtput->ReplayBufferActive();
}

bool PLSOutputHandler::VirtualCamActive() const
{
	return houtput->VirtualCamActive();
}

void PLSOutputHandler::Update()
{
	houtput->Update();
}

void PLSOutputHandler::UpdateVirtualCamOutputSource()
{
	houtput->UpdateVirtualCamOutputSource();
}

bool PLSOutputHandler::Active() const
{
	return houtput->Active() || (voutput && voutput->Active());
}

bool PLSOutputHandler::streamingActive() const
{
	return houtput->streamingActive || (voutput && voutput->streamingActive);
}

bool PLSOutputHandler::streamingActive(StreamingType streamType) const
{
	switch (streamType) {
	case StreamingType::Horizontal:
		return houtput->streamingActive;
	case StreamingType::Vertical:
		return voutput && voutput->streamingActive;
	default:
		assert(false && "It's unexpected");
		return false;
	}
}

bool PLSOutputHandler::replayBufferActive() const
{
	return houtput->replayBufferActive;
}

bool PLSOutputHandler::virtualCamActive() const
{
	return houtput->virtualCamActive;
}

void PLSOutputHandler::StreamDelayStarting(BasicOutputHandler *handler, int sec)
{
	auto type = houtput.get() == handler ? StreamingType::Horizontal : StreamingType::Vertical;
	streamDelayStarting[type] = true;
	streamDelayStartingSec[type] = sec;
	if ((!startStreaming[Horizontal] || streamDelayStarting[Horizontal]) && (!startStreaming[Vertical] || streamDelayStarting[Vertical])) {
		QMetaObject::invokeMethod(main, "StreamDelayStarting", Q_ARG(int, streamDelayStartingSec[Horizontal]), Q_ARG(int, streamDelayStartingSec[Vertical]));
	}
}
void PLSOutputHandler::StreamDelayStopping(BasicOutputHandler *handler, int sec)
{
	auto type = houtput.get() == handler ? StreamingType::Horizontal : StreamingType::Vertical;
	streamDelayStopping[type] = true;
	streamDelayStoppingSec[type] = sec;
	if ((!startStreaming[Horizontal] || streamDelayStopping[Horizontal]) && (!startStreaming[Vertical] || streamDelayStopping[Vertical])) {
		QMetaObject::invokeMethod(main, "StreamDelayStopping", Q_ARG(int, streamDelayStoppingSec[Horizontal]), Q_ARG(int, streamDelayStoppingSec[Vertical]));
	}
}

void PLSOutputHandler::StreamingStart(BasicOutputHandler *handler)
{
	auto type = houtput.get() == handler ? StreamingType::Horizontal : StreamingType::Vertical;
	streamingStart[type] = true;
	if ((!streamingStartInvoked) && (!startStreaming[Horizontal] || streamingStart[Horizontal] || streamingStop[Horizontal]) &&
	    (!startStreaming[Vertical] || streamingStart[Vertical] || streamingStop[Vertical])) {
		streamingStartInvoked = true;

		if (streamingStop[Horizontal]) {
			PLS_PLATFORM_API->addFailedPlatform(PLS_PLATFORM_HORIZONTAL);
			if (auto message =
				    static_cast<PLSBasic *>(main)->getOutputStreamErrorAlert(streamingStopErrorCode[StreamingType::Horizontal], streamingStopLastError[StreamingType::Horizontal]);
			    !message.isEmpty()) {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(PLSPlatformApi::joinPlatformNames(PLS_PLATFORM_HORIZONTAL), message));
			}
			streamingStopErrorCode[StreamingType::Horizontal] = 0;
			streamingStopLastError[StreamingType::Horizontal].clear();
		} else if (streamingStop[Vertical]) {
			PLS_PLATFORM_API->addFailedPlatform(PLS_PLATFORM_VERTICAL);
			if (auto message = static_cast<PLSBasic *>(main)->getOutputStreamErrorAlert(streamingStopErrorCode[StreamingType::Vertical], streamingStopLastError[StreamingType::Vertical]);
			    !message.isEmpty()) {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(PLSPlatformApi::joinPlatformNames(PLS_PLATFORM_VERTICAL), message));
			}
			streamingStopErrorCode[StreamingType::Vertical] = 0;
			streamingStopLastError[StreamingType::Vertical].clear();
		}

		QMetaObject::invokeMethod(main, "StreamingStart");
	}
}

void PLSOutputHandler::StreamStopping(BasicOutputHandler *handler)
{
	auto type = houtput.get() == handler ? StreamingType::Horizontal : StreamingType::Vertical;
	streamStopping[type] = true;
	if ((!startStreaming[Horizontal] || streamStopping[Horizontal]) && (!startStreaming[Vertical] || streamStopping[Vertical])) {
		QMetaObject::invokeMethod(main, "StreamStopping");
	}
}
void PLSOutputHandler::StreamingStop(BasicOutputHandler *handler, int errorcode, QString last_error)
{
	auto type = houtput.get() == handler ? StreamingType::Horizontal : StreamingType::Vertical;
	streamingStop[type] = true;
	streamingStopErrorCode[type] = errorcode;
	streamingStopLastError[type] = last_error;

	if ((!streamingStartInvoked) && (streamingStart[Horizontal] || streamingStart[Vertical]) && (!startStreaming[Horizontal] || streamingStart[Horizontal] || streamingStop[Horizontal]) &&
	    (!startStreaming[Vertical] || streamingStart[Vertical] || streamingStop[Vertical])) {
		streamingStartInvoked = true;

		auto platforms = StreamingType::Horizontal == type ? PLS_PLATFORM_HORIZONTAL : PLS_PLATFORM_VERTICAL;
		PLS_PLATFORM_API->addFailedPlatform(platforms);
		if (auto message = static_cast<PLSBasic *>(main)->getOutputStreamErrorAlert(errorcode, last_error); !message.isEmpty()) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(PLSPlatformApi::joinPlatformNames(platforms), message));
		}
		streamingStopErrorCode[type] = 0;
		streamingStopLastError[type].clear();

		QMetaObject::invokeMethod(main, "StreamingStart");
	}

	if ((!startStreaming[Horizontal] || streamingStop[Horizontal]) && (!startStreaming[Vertical] || streamingStop[Vertical])) {
		QMetaObject::invokeMethod(main, "StreamingStop", Q_ARG(int, streamingStopErrorCode[Horizontal]), Q_ARG(QString, streamingStopLastError[Horizontal]),
					  Q_ARG(int, streamingStopErrorCode[Vertical]), Q_ARG(QString, streamingStopLastError[Vertical]));
	}
}

void PLSOutputHandler::RecordingStart(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "RecordingStart");
}
void PLSOutputHandler::RecordStopping(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "RecordStopping");
}
void PLSOutputHandler::RecordingStop(BasicOutputHandler *handler, int code, QString last_error)
{
	QMetaObject::invokeMethod(main, "RecordingStop", Q_ARG(int, code), Q_ARG(QString, last_error));
}
void PLSOutputHandler::RecordingFileChanged(BasicOutputHandler *handler, QString lastRecordingPath)
{
	QMetaObject::invokeMethod(main, "RecordingFileChanged", Q_ARG(QString, lastRecordingPath));
}

void PLSOutputHandler::ReplayBufferStart(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "ReplayBufferStart");
}
void PLSOutputHandler::ReplayBufferSaved(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "ReplayBufferSaved");
}
void PLSOutputHandler::ReplayBufferStopping(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "ReplayBufferStopping");
}
void PLSOutputHandler::ReplayBufferStop(BasicOutputHandler *handler, int code)
{
	QMetaObject::invokeMethod(main, "ReplayBufferStop", Q_ARG(int, code));
}

void PLSOutputHandler::OnVirtualCamStart(BasicOutputHandler *handler)
{
	QMetaObject::invokeMethod(main, "OnVirtualCamStart");
}
void PLSOutputHandler::OnVirtualCamStop(BasicOutputHandler *handler, int code)
{
	QMetaObject::invokeMethod(main, "OnVirtualCamStop", Q_ARG(int, code));
}
