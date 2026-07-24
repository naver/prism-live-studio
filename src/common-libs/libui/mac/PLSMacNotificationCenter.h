#ifndef PLSMACNOTIFICATIONCENTER_H
#define PLSMACNOTIFICATIONCENTER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace pls {
namespace mac {
using signalCallback = std::function<void(const QString &signalName, const QJsonObject &payload)>;

constexpr const char *PLS_PRISM_ACTIVE_SIGNAL_NAME = "com.prism.prismlivestudio.notify";
constexpr const char *PLS_LENS_ACTIVE_SIGNAL_NAME = "com.prismlive.camstudio.notify";
#if defined(PRODUCT_PRISM)
constexpr const char *PLS_MAC_ACTIVE_SIGNAL_NAME = PLS_PRISM_ACTIVE_SIGNAL_NAME;
#elif defined(PRODUCT_LENS)
constexpr const char *PLS_MAC_ACTIVE_SIGNAL_NAME = PLS_LENS_ACTIVE_SIGNAL_NAME;
#else
constexpr const char *PLS_MAC_ACTIVE_SIGNAL_NAME = PLS_PRISM_ACTIVE_SIGNAL_NAME;
#endif

void listenSignal(const QString &signalName, const signalCallback &callback);
void sendSignal(const QString &signalName, const QJsonObject &payload = {});
void removeSignalListener(const QString &signalName);
void removeAllSignalListeners();
}
}

#endif // PLSMACNOTIFICATIONCENTER_H
