#ifndef CHANNEL_DEFINE_H
#define CHANNEL_DEFINE_H

#include <qmetatype.h>
#include <QListWidgetItem>
#include <functional>
#include "ChannelCapsule.h"
#include "ChannelFoldCapsule.h"

namespace channel_data {

#ifndef ChannelData
#define ChannelData channel_data
#endif // !ChannelData

using ItemPtr = QSharedPointer<QListWidgetItem>;
using ChannelsFunction = void (*)(QVariantMap &);
using ChannelCapsulePtr = QSharedPointer<ChannelCapsule>;
using ChannelFoldCapsulePtr = QSharedPointer<ChannelFoldCapsule>;
using TaskFun = std::function<void(const QVariant &)>;
using TaskFunNoPara = std::function<void(void)>;

}
Q_DECLARE_METATYPE(channel_data::ItemPtr)
Q_DECLARE_METATYPE(channel_data::ChannelsFunction)
Q_DECLARE_METATYPE(channel_data::ChannelCapsulePtr)
Q_DECLARE_METATYPE(channel_data::TaskFun)
Q_DECLARE_METATYPE(channel_data::ChannelFoldCapsulePtr)

#endif // !CHANNEL_DEFINE_H
