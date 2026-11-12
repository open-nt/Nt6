// MIT License - see LICENSE
//
// Overview page about Nt Qml for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-qml.html
    \title Using Nt Qml
    \brief How to run JavaScript in an application, and let scripts use its QObjects.

    Nt Qml runs JavaScript. QJSEngine evaluates programs, QJSValue holds what they make, and QObjects take
    part in scripts with their properties, methods and signals. It is the base of QML, which follows.

    \code
    find_package(Nt6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt::Qml)
    \endcode

    \section1 Running Code

    evaluate() runs a program and returns the value of its last statement. Functions come back as values
    that C++ can call:

    \snippet qml/scripting.cpp evaluate

    Errors do not escape as C++ exceptions: evaluate() and QJSValue::call() return the error, an object
    with \c name, \c message, \c fileName, \c lineNumber and \c stack.

    \section1 QObjects in Scripts

    newQObject() gives scripts a QObject. They read and write its Q_PROPERTYs, call its slots and
    Q_INVOKABLE methods, emit its signals by calling them, and connect them to functions with
    \c{signal.connect(f)} or \c{signal.connect(thisObject, f)}:

    \snippet qml/scripting.cpp qobject

    Enumeration properties take their keys as strings, \c{"Heating"}, or their values. Overloaded methods are
    chosen by the number and types of the arguments. Properties that scripts add to a QObject are kept on
    its script value. A script calling a method of a deleted object gets a TypeError.

    \section1 The Language

    Nt implements a large part of ECMAScript 2017, interpreted:

    \list
    \li \c var, \c let and \c const, with block scopes and closures that capture each loop iteration;
    \li functions, arrow functions, default and rest parameters, spread arguments and array elements;
    \li object literals with methods, getters, setters, computed keys and spread; prototypes and \c new;
    \li \c ?., \c ??, \c **, logical assignment, template literals and regular expression literals;
    \li \c for-in, \c for-of over arrays and strings, labels, \c switch, \c try with \c finally;
    \li Object, Function, Array, String, Number, Boolean, Math, JSON, Date, RegExp, the errors, and the
        global functions; \c console and \c print with QJSEngine::ConsoleExtension.
    \endlist

    Not supported: classes, generators, async functions and promises, destructuring, symbols and iterators
    (for-of works on arrays, strings and array-likes), Map and Set, Proxy and Reflect, typed arrays,
    getters on arrays, tagged templates, \c with, and making functions from strings. Arrays have no holes:
    writing past the end fills the gap with undefined. Regular expressions run on QRegularExpression,
    whose syntax is that of JavaScript for the common cases.

    \section1 Untrusted Scripts

    Source code is treated as untrusted input. Nesting in the source, recursion at run time and the size
    of arrays and strings are limited, so a hostile script fails with an error instead of exhausting the
    stack or memory. QJSEngine::setInterrupted() stops a script that runs too long; call it from a timer
    or another thread. A script can do only what the objects given to it allow: there is no file or
    network access of its own.

    The \c js_scripting example runs the snippets above, or a file given on its command line.
*/
