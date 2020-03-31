#ifndef CHANNEL_DEFINE_H
#define CHANNEL_DEFINE_H

#include <QMetaType>
#include <QListWidgetItem>
#include "ChannelCapsule.h"

namespace ChannelData {

using ItemPtr = QSharedPointer<QListWidgetItem>;
Q_DECLARE_METATYPE(ItemPtr)

using ChannelsFunction = void (*)(QVariantMap &);
Q_DECLARE_METATYPE(ChannelsFunction)

using ChannelCapsulePtr = QSharedPointer<ChannelCapsule>;
Q_DECLARE_METATYPE(ChannelCapsulePtr)
}

#endif // !CHANNEL_DEFINE_H
