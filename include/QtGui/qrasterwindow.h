// MIT License - see LICENSE
#pragma once

#include <QtGui/qpaintdevicewindow.h>

class QRasterWindow : public QPaintDeviceWindow {
    Q_OBJECT
public:
    explicit QRasterWindow(QWindow *parent = nullptr);
    ~QRasterWindow() override;
};
