// MIT License - see LICENSE
//
// The platform part of QGpioLine. qgpiobackend_linux.cpp talks to the GPIO character device of
// Linux and src/zephyr/qgpiobackend_zephyr.cpp to the GPIO drivers of Zephyr; each defines
// createPlatformGpioBackend(). The tests replace it with setGpioBackendFactory().
#pragma once

#include <QtGpio/qgpioline.h>

#include <memory>
#include <vector>

namespace QtPrivate {

struct GpioConfig {
    QString name;
    QGpioLine::Direction direction = QGpioLine::Input;
    QGpioLine::Bias bias = QGpioLine::AsIs;
    bool activeLow = false;
    bool initialValue = false; // the logical value an output line starts with
};

struct GpioFailure {
    QGpioLine::GpioError error = QGpioLine::NoError;
    QString message;
};

// Fills failure from the errno value code, with what as the start of the message.
void setGpioFailure(GpioFailure &failure, int code, const QString &what);

class GpioBackend {
public:
    GpioBackend() = default;
    virtual ~GpioBackend() = default;
    GpioBackend(const GpioBackend &) = delete;
    GpioBackend &operator=(const GpioBackend &) = delete;

    // Requests the line. Values are logical: active-low lines are inverted by the backend.
    virtual bool open(const GpioConfig &config, GpioFailure &failure) = 0;
    // A descriptor that becomes readable when an input line has changed, or -1 if the line must be
    // polled.
    virtual int eventDescriptor() const = 0;
    // Consumes what made eventDescriptor() readable and returns the levels the line went through, in
    // order. A backend that cannot tell the edges apart returns the current level.
    virtual bool readEvents(std::vector<bool> &levels, GpioFailure &failure) = 0;
    virtual bool readValue(bool &value, GpioFailure &failure) = 0;
    virtual bool writeValue(bool value, GpioFailure &failure) = 0;
};

using GpioBackendFactory = std::unique_ptr<GpioBackend> (*)();

std::unique_ptr<GpioBackend> createPlatformGpioBackend();
// Makes QGpioLine::open() use factory, or the platform backend again when factory is null.
void setGpioBackendFactory(GpioBackendFactory factory);

} // namespace QtPrivate
