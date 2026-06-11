#pragma once
#include <QString>
#include <memory>
#include <QRect>
#include <QPointer>
#include <qjsonobject.h>

#ifdef _DEBUG
#define TEST_OUTRO 0
#endif

namespace b2b_service {
constexpr auto VLOGO_FIX_HEIGHT_PIX = 480.0f;
constexpr auto VLOGO_MAX_WIDTH_PIX = 960.0f;

constexpr auto HLOGO_FIX_HEIGHT_PIX = 240.0f;
constexpr auto HLOGO_MAX_WIDTH_PIX = 1950.0f;
};

namespace b2b_local {
constexpr auto VLOGO_FIX_HEIGHT_PIX = 160.0f;
constexpr auto VLOGO_MAX_WIDTH_PIX = 320.0f;

constexpr auto HLOGO_FIX_HEIGHT_PIX = 80.0f;
constexpr auto HLOGO_MAX_WIDTH_PIX = 650.0f;
};

enum class CustomChannel : uint32_t {
	CHANNEL_WATERMARK = 20,
	CHANNEL_OUTRO,
};

enum class OutroState {
	Started,
	Stopped,
};

using OutroStateCallback = std::function<void(OutroState state)>;
class PLSOutro {
public:
	/*
	* settings: outro parameters, inlcude image path, text content, text font family, font size, and image & text goemetry. etc.
	*/
	PLSOutro(const QJsonObject &settings, uint32_t channel = static_cast<uint32_t>(CustomChannel::CHANNEL_OUTRO), OutroStateCallback callback = {});
	virtual ~PLSOutro() = default;

	virtual void Update(const QJsonObject &settings) = 0;
	virtual bool IsRunning() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;

protected:
	uint32_t m_channel;
	QJsonObject m_settings;
	OutroStateCallback m_callback;
	int m_durationMs = 0;
};

std::shared_ptr<PLSOutro> pls_create_outro(const QJsonObject &settings, CustomChannel channel, OutroStateCallback callback);
