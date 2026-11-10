// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <exception>

class QException : public std::exception {
public:
    QException() = default;
    QException(const QException &) = default;
    QException &operator=(const QException &) = default;
    ~QException() noexcept override;

    virtual void raise() const;
    virtual QException *clone() const;
};

class QUnhandledException : public QException {
public:
    QUnhandledException(std::exception_ptr exception = nullptr) noexcept;
    QUnhandledException(const QUnhandledException &other) noexcept;
    QUnhandledException &operator=(const QUnhandledException &other) noexcept;
    ~QUnhandledException() noexcept override;

    void raise() const override;
    QUnhandledException *clone() const override;
    std::exception_ptr exception() const;

private:
    std::exception_ptr m_exception;
};
