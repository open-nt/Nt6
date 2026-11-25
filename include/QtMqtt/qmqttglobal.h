// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>

namespace QMqtt {

enum class MessageStatus : quint8 { Unknown = 0, Published, Acknowledged, Received, Released, Completed };

} // namespace QMqtt

Q_DECLARE_METATYPE(QMqtt::MessageStatus)
