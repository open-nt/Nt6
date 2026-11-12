// MIT License - see LICENSE
//
// Overview page about QML in Nt for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-qml-language.html
    \title Using Nt QML
    \brief How to declare objects in QML documents, bind their properties, and give QML your C++ types.

    QML describes a tree of objects: their properties, the bindings that keep properties up to date, and
    what happens when signals arrive. Nt Qml reads QML documents and makes their objects with QQmlEngine
    and QQmlComponent, or with QQmlApplicationEngine for the main document of an application.

    \code
    find_package(Nt6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt::Qml)
    \endcode

    \section1 A Document

    \code
    import QtQml

    QtObject {
        id: root
        property int hour: 6
        property bool night: hour < 7 || hour > 21     // a binding: kept up to date
        signal alarm(string reason)
        onAlarm: (reason) => console.log("alarm:", reason)
        function later() { hour += 1 }
        property Timer clock: Timer { interval: 1000; repeat: true; running: true; onTriggered: root.later() }
        Component.onCompleted: console.log("ready")
    }
    \endcode

    A document imports modules, then declares one root object. Objects have an \c id, properties (their
    type's, and the ones they declare with \c property), signals, functions, enumerations, and child
    objects, which go to the default property of their parent. Properties are set to values or bound to
    expressions; \c on handlers run when signals arrive.

    \section1 Making the Objects

    QQmlComponent reads a document and makes its objects:

    \snippet qml/hello_qml.cpp component

    QQmlApplicationEngine loads the main document of an application, and quits it when the document calls
    \c{Qt.quit()}:

    \snippet qml/hello_qml.cpp application

    \section1 Bindings

    A property set to an expression is bound to it: the expression is evaluated again whenever a
    property it read changes. That can be a C++ property with a NOTIFY signal, a property declared in QML,
    or a context property. Assigning a value from a script replaces the binding; \c{Qt.binding()} makes a
    new one. A binding that changes what it depends on is reported as a binding loop.

    Errors in bindings and handlers are warnings: QQmlEngine::warnings() reports them, with the file and
    line, and the application goes on.

    \section1 C++ Types

    qmlRegisterType() makes a QObject class a QML type of a module:

    \snippet qml/hello_qml.cpp register

    Documents see its Q_PROPERTYs, signals, slots, Q_INVOKABLE methods and Q_ENUMs (as
    \c{Thermostat.SomeValue}). \c{Q_CLASSINFO("DefaultProperty", "name")} names the property that child
    objects go to; a QQmlListProperty property takes several. A class that inherits QQmlParserStatus is
    told when its component is complete. qmlRegisterSingletonType() and qmlRegisterSingletonInstance()
    make singletons, used by type name. qmlRegisterUncreatableType() makes a type that documents use but
    cannot make.

    The \c QML_ELEMENT macros compile but do nothing, as Nt has no build step that reads them: register
    the types with these functions instead.

    \section1 Types Written in QML

    A file \c{Badge.qml} in the directory of a document is the type \c Badge there. \c{import "dir"}
    imports a directory of QML files, \c{import Module} a module from the import paths of the engine
    (QQmlEngine::addImportPath()), described by a \c qmldir file:

    \code
    module Widgets
    Panel 1.0 Panel.qml
    singleton Style 1.0 Style.qml
    \endcode

    \c{component Name: Type { }} declares an inline component, and \c{Component { }} a component that
    scripts make objects from with \c{createObject(parent, properties)}.

    \section1 The QtQml Module

    \c QtObject, \c Timer, \c Connections and \c Component, and the \c Qt object: \c{Qt.binding()},
    \c{Qt.callLater()}, \c{Qt.quit()}, \c{Qt.exit()}, \c{Qt.point()}, \c{Qt.size()}, \c{Qt.rect()},
    \c{Qt.formatDate()}, \c{Qt.formatTime()}, \c{Qt.formatDateTime()}, \c{Qt.createComponent()},
    \c{Qt.createQmlObject()}, \c{Qt.resolvedUrl()}, \c{Qt.md5()}, \c{Qt.btoa()}, \c{Qt.atob()},
    \c{Qt.platform.os} and \c{Qt.application}. The JavaScript of documents is that of \l{Using Nt Qml}.

    \section1 Not Yet Supported

    Visual types (Qt Quick), attached properties other than \c{Component.onCompleted} and
    \c{Component.onDestruction}, \c Behavior and other "on" assignments, \c Binding, \c Loader,
    \c Repeater, models and views, JavaScript files imported by documents, required properties being
    checked, translations, and network URLs. Documents are interpreted, not compiled to C++.

    The \c hello_qml example runs a document with a C++ type, a Timer and bindings.
*/
