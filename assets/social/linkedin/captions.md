# Nt Explained: LinkedIn captions

Each caption is also in a `.txt` file next to its image, with the same name. Post one image per day (or one a week), in order. Each caption goes with the image of the same number, 1080 x 1350 (4:5).
Put the link in the first comment if you want the post to reach further, or leave it in the text.

Repository: https://github.com/open-nt/Nt6
Documentation: https://open-nt.github.io/Nt6/

---

## 1. What is Nt? (`nt-linkedin-1-what-is-nt.png`)

If you write C++ with Qt, meet Nt ("Nut"): a small, clean-room implementation of the Qt 6 API under the MIT license.

You keep writing the code you know: QObject, signals and slots, QString, QTimer, sockets, widgets. Nt builds with CMake and a C++17 compiler.

It comes in nine modules (Core, Network, Serial Port, D-Bus, HTTP Server, WebSockets, GUI, Widgets and Test), and it grows one module at a time.

Over the next eight posts I'll show how it works, one lesson each. Follow along, and tell me which part of the Qt API you would want first.

https://github.com/open-nt/Nt6

#cpp #opensource #qt #cmake #linux

---

## 2. Clean-room and MIT, explained (`nt-linkedin-2-clean-room-mit.png`)

Two words in Nt's description deserve a proper explanation.

Clean-room: Nt is written without copying Qt's source code. It follows the public Qt 6 API, so your code keeps its shape.

MIT: a short, permissive license. You can use, change and ship Nt in open or closed source products, as long as you keep the license notice.

And one thing to be clear about: Nt is not affiliated with The Qt Company, and its name and logo are not meant to be mistaken for Qt's.

https://github.com/open-nt/Nt6

#opensource #mitlicense #cpp #qt

---

## 3. Signals and slots, no moc (`nt-linkedin-3-signals-slots.png`)

Signals and slots are the heart of Qt code. In Nt they work the way you expect: Q_OBJECT, signals:, slots:, emit and pointer-to-member connect().

The difference is the tool behind them. Instead of Qt's moc, Nt uses ntmoc, a small MIT scanner that generates the bodies of the signals you declare. It also accepts the command line that CMake's AUTOMOC uses, so it plugs into your build.

Your class stays exactly as you would write it for Qt.

https://github.com/open-nt/Nt6

#cpp #qt #cmake #opensource

---

## 4. Nine modules, built one by one (`nt-linkedin-4-modules.png`)

Qt is big. Nt is growing a module at a time: Core, Network, Serial Port, D-Bus, HTTP Server, WebSockets, GUI, Widgets and Test.

Four of them (Core, Network, GUI and Widgets) are marked partial: the classes Nt needs first are there, and the rest is coming.

The part I like most: every class documents what differs from Qt, in a "Differences from Qt" section. No surprises hiding in the details.

https://open-nt.github.io/Nt6/

#cpp #qt #opensource #documentation

---

## 5. Three lines of CMake (`nt-linkedin-5-cmake.png`)

Using Nt in a project takes three lines:

find_package(Nt6 6 REQUIRED)
target_link_libraries(app PRIVATE Nt6::Network)
nt_enable_automoc(app)

Both Nt:: and Qt6:: target names work. Headers install to /usr/include/nt6, so Nt can sit next to Qt on the same machine. And there are packages for Ubuntu, Enterprise Linux, Arch and snap.

https://github.com/open-nt/Nt6

#cmake #cpp #linux #packaging

---

## 6. A web service in six lines (`nt-linkedin-6-web-service.png`)

An HTTP service with Nt HTTP Server:

QHttpServer server;
server.route("/hello/<arg>", [](const QString &name) { return "Hello, " + name; });
server.bind(tcp);  // a QTcpServer

Routes take typed arguments (<arg> arrives as a QString, an int or a double), and the server can listen on TCP, TLS (HTTPS) and local sockets.

Same names as Qt, with Nt underneath.

https://github.com/open-nt/Nt6

#cpp #qt #webdevelopment #opensource

---

## 7. Widgets, the Qt way (`nt-linkedin-7-widgets.png`)

Nt Widgets is the newest part: labels, buttons, line edits, sliders, spin boxes, tables, lists, text editors, menus and more, drawn with a Fusion-like style.

The hello_widgets example shows one of every widget, live, on its first tab. It follows your desktop's dark or light theme, and a CMake option (-DNT_EXAMPLES_QT=ON) builds the same example against real Qt, so you can compare the two side by side.

Run it and see.

https://github.com/open-nt/Nt6

#cpp #qt #gui #linux #opensource

---

## 8. Trust it, improve it (`nt-linkedin-8-contribute.png`)

Why trust a young library? Because of what happens before every commit: ./check.sh builds with -Werror, runs clang-tidy and cppcheck, runs the tests, checks that every public class is documented, and repeats the tests under AddressSanitizer, UBSan and ThreadSanitizer. CI builds Nt on Ubuntu 22.04, 24.04 and 26.04, Enterprise Linux 9 and 10, and Arch Linux.

And it is open: star it, fork it, send a pull request. Contributors are welcome.

https://github.com/open-nt/Nt6

#opensource #cpp #contributing #linux
