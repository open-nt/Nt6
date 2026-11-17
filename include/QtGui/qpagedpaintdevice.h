// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtGui/qpagelayout.h>
#include <QtGui/qpaintdevice.h>

class QPagedPaintDevice : public QPaintDevice {
public:
    enum PdfVersion { PdfVersion_1_4, PdfVersion_A1b, PdfVersion_1_6 };

    ~QPagedPaintDevice() override;

    virtual bool newPage() = 0;

    virtual bool setPageLayout(const QPageLayout &pageLayout);
    virtual bool setPageSize(const QPageSize &pageSize);
    virtual bool setPageOrientation(QPageLayout::Orientation orientation);
    virtual bool setPageMargins(const QMarginsF &margins, QPageLayout::Unit units = QPageLayout::Millimeter);
    QPageLayout pageLayout() const { return m_layout; }

protected:
    QPagedPaintDevice();

private:
    Q_DISABLE_COPY(QPagedPaintDevice)
    QPageLayout m_layout;
};
