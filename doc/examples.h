// MIT License - see LICENSE
//
// Overview page about the Nt example applications for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page examples.html
    \title Nt Examples
    \brief Example applications built against an installed Nt, from a hello world to a platform game and a
    real-time path tracer.

    The example applications live in a repository of their own,
    \l{https://github.com/open-nt/nt6-examples}{open-nt/nt6-examples}, set up to build with CMake against an
    installed Nt, the way your own project would. The guides in this documentation walk through most of them.

    \section1 Building the Examples

    Install Nt, for instance from the snap (see \l{install.html}{Installing Nt}), and the packages its static
    libraries link against, then build and test everything:

    \badcode
    sudo snap install open-nt6
    sudo apt install cmake g++ libssl-dev libfreetype-dev libfontconfig-dev libxcb1-dev
    git clone https://github.com/open-nt/nt6-examples.git
    cd nt6-examples
    cmake -S . -B build
    cmake --build build
    ctest --test-dir build
    \endcode

    The snap's location, \c{/snap/open-nt6/current/usr}, is searched by default. For an Nt installed
    elsewhere, pass \c{-DCMAKE_PREFIX_PATH=<prefix>}. Without a display, run the graphical examples with
    \c{-platform offscreen}.

    \section1 The Examples

    \table
    \header \li Directory \li Program \li Shows
    \row \li \c hello/ \li \c hello \li A QCoreApplication and a timer
    \row \li \c signals/ \li \c signals \li The signal and slot patterns
    \row \li \c moc/ \li \c moc_example \li \c Q_OBJECT classes processed by \c ntmoc, as with automoc
    \row \li \c test/ \li \c tst_counter \li A test written with Nt Test
    \row \li \c gui/ \li \c hello_window, \c painting, \c painter_bench \li A window painted with QPainter, the
         scenes of the painting guide (\c{painting --write dir} renders its pictures), and a QPainter benchmark
    \row \li \c widgets/ \li \c hello_widgets, \c gallery, \c render_widgets \li A form, a tour of the widgets,
         and widgets rendered to PNG
    \row \li \c svg/ \li \c svg_viewer \li Showing and converting an SVG file
    \row \li \c opengl/ \li \c glwindow \li A window rendered with OpenGL and QPainter
    \row \li \c vulkan/ \li \c rtriangle \li A triangle traced with the Vulkan ray tracing pipeline
    \row \li \c charts/ \li \c chart_dashboard, \c render_charts \li A live dashboard of charts, and the pictures
         of the charts guide
    \row \li \c qml/ \li \c hello_qml, \c js_scripting \li A QML document with a C++ type, and scripting with
         QJSEngine
    \row \li \c sql/ \li \c contacts \li An address book in an SQLite database
    \row \li \c serialport/ \li \c serialterm \li A serial terminal
    \row \li \c websockets/ \li \c websocket_echo \li A WebSocket echo server and client
    \row \li \c httpserver/ \li \c hello_server \li A small web service
    \row \li \c dbus/ \li \c dbus_clock \li A clock on the D-Bus session bus
    \row \li \c gpio/ \li \c ptt \li A push-to-talk switch on a GPIO line
    \row \li \c gamepad/ \li \c gamepadinfo \li What the gamepads do
    \row \li \c gltf/ \li \c gltfinfo \li What a glTF file holds
    \row \li \c multimedia/ \li \c audiodevices, \c audioplayer, \c decoder, \c recorder, \c camera, \c viewfinder
         \li Audio devices and playback, decoding, recording, and the camera
    \row \li \c fm/ \li \c fmplay, \c fmqml \li FM sound effects and chip music, from C++ and from QML
    \row \li \c zephyr/ \li \c hello, \c qemu \li Nt Core and Nt Network on Zephyr: a board application, and a
         self-checking one for QEMU, built with west (see \l{Nt on Zephyr})
    \row \li \c platformer/ \li \c mochimaru, \c sprite_sheet \li A platform game: Nt Gamepad, Nt FM sound, a
         MIDI soundtrack and an OpenGL window
    \row \li \c raytracing/ \li \c pathtracer \li A path tracer on the Vulkan ray tracing pipeline in a
         QVulkanWindow
    \row \li \c qt/ \li \c hello_widgets, \c gallery \li The same sources built against Qt 6, for comparison
    \endtable

    \section1 Mochimaru no Daibouken

    \image examples/mochimaru.png Mochimaru no Daibouken: the bunny in 1-2 Bamboo Grove, with a pipe and enemies

    \c platformer/ is a small platform game. The controls come from Nt Gamepad, with the keyboard as a
    fallback, and the sound effects and soundtrack are played by the FM synthesizer of
    \l{nt-fm.html}{Nt FM}, drawn into an OpenGL window. Run \c{build/mochimaru}, or
    \c{build/mochimaru song.mid} to play with your own MIDI soundtrack (several files, or a directory of them,
    work too); \c{mochimaru --export-song tune.mid} writes the built-in tune as a starting point. The pixel art
    is text, in \c{platformer/sprites.h}, and \c sprite_sheet draws all of it on one sheet for review.

    \table
    \header \li Action \li Gamepad \li Keyboard
    \row \li Move \li D-pad or left stick \li Arrows or WASD
    \row \li Jump \li A \li Space, Z or Up
    \row \li Run \li X or B \li X or Shift
    \row \li Pause / start \li Start \li Esc or Enter
    \row \li Music on/off \li Select \li M
    \row \li Next song \li Y \li N
    \row \li Fullscreen \li \li Alt+Enter or F11
    \endtable

    \section1 Path Tracer

    \image examples/pathtracer.jpg The path tracer: spheres of glass, metal and colour on a checkered floor

    \c raytracing/ renders a scene of spheres and boxes with a progressive path tracer on the Vulkan ray
    tracing pipeline (\c VK_KHR_ray_tracing_pipeline) in a QVulkanWindow; see \l{nt-vulkan.html}{Using Nt
    Vulkan}. It is built the way an RTX game pipeline is: a few paths per pixel per frame, up to 16 bounces,
    direct light chosen by resampled importance sampling, per-pixel history reprojected through the motion of
    the camera and the objects, and an à-trous wavelet denoiser, so the picture stays smooth while everything
    moves and converges exactly when it rests. It is an Nt-only example, with no Qt 6 counterpart.

    On an RTX GPU it also uses shader execution reordering
    (\c VK_NV_ray_tracing_invocation_reorder; press R to compare frame rates), ray tracing position fetch,
    and, in a build against NVIDIA's separately licensed DLSS SDK, DLSS Ray Reconstruction (\c{--dlss} or the N
    key). It needs a GPU with ray tracing (NVIDIA RTX, AMD RDNA 2 or later, Intel Arc), or Mesa's \c lavapipe
    for a slow software run, and an Nt built with the Vulkan headers.

    Run \c{build/pathtracer}, or \c{build/pathtracer --fly} to let the camera circle the scene. Drag with the
    left button to orbit, use the wheel to zoom, Space to pause the motion and let the image converge, D to
    switch the denoiser, + and - to change the bounces, S to save a screenshot and Q to quit. The
    \l{https://github.com/open-nt/nt6-examples#path-tracer}{repository's README} lists every option.

    \sa {Installing Nt}, {Using Nt Vulkan}, {Using Nt FM}
*/
