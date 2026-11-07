#pragma once

class Base {
public:
    explicit Base(Base *parent = nullptr);
    virtual ~Base();

    int value() const;

protected:
    virtual void handle(int code);
};

/*!
    \internal
*/
class Detail {
public:
    void hidden();
};
