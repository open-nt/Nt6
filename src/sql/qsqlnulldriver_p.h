// MIT License - see LICENSE
#pragma once

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlresult.h>

// The result of a query that has no usable driver: every operation fails.
class QSqlNullResult : public QSqlResult {
public:
    explicit QSqlNullResult(const QSqlDriver *driver) : QSqlResult(driver) {
        QSqlResult::setLastError(QSqlError(QStringLiteral("Driver not loaded"), QStringLiteral("Driver not loaded"),
                                           QSqlError::ConnectionError));
    }

protected:
    QVariant data(int) override { return QVariant(); }
    bool isNull(int) override { return false; }
    bool reset(const QString &) override { return false; }
    bool fetch(int) override { return false; }
    bool fetchFirst() override { return false; }
    bool fetchLast() override { return false; }
    int size() override { return -1; }
    int numRowsAffected() override { return 0; }
    bool exec() override { return false; }
    bool prepare(const QString &) override { return false; }
    void setLastError(const QSqlError &) override {} // keeps "Driver not loaded"
};

// The driver of a QSqlDatabase handle that has no connection, or whose kind of database has no driver.
class QSqlNullDriver : public QSqlDriver {
public:
    QSqlNullDriver() {
        QSqlDriver::setLastError(QSqlError(QStringLiteral("Driver not loaded"), QStringLiteral("Driver not loaded"),
                                           QSqlError::ConnectionError));
    }
    bool hasFeature(DriverFeature) const override { return false; }
    bool open(const QString &, const QString &, const QString &, const QString &, int, const QString &) override {
        return false;
    }
    void close() override {}
    QSqlResult *createResult() const override { return new QSqlNullResult(this); }

protected:
    void setOpen(bool) override {}
    void setOpenError(bool) override {}
    void setLastError(const QSqlError &) override {}
};
