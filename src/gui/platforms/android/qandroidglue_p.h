// MIT License - see LICENSE
#pragma once

#include <android/configuration.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/rect.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace QtAndroid {

// What the UI thread of the activity tells the thread that runs main().
struct Event {
    enum Kind {
        Touch,         // action is an AMOTION_EVENT_ACTION_* value; x and y are in pixels
        Key,           // action is AKEY_EVENT_ACTION_*; keyCode and meta as Android has them
        WindowCreated, // the native window is available (Glue::window())
        WindowDestroyed,
        WindowResized,
        ConfigChanged,
        Text, // text typed on the on-screen keyboard (text)
        Pause,
        Resume,
        Close // the activity is being destroyed
    };
    Kind kind = Touch;
    int action = 0;
    int keyCode = 0;
    int meta = 0;
    float x = 0;
    float y = 0;
    int pointerId = 0;
    std::string text;
};

// The bridge between android.app.NativeActivity, whose callbacks run on the UI thread, and the thread that runs
// the application's main(). The events cross in a queue, and a pipe that the event loop of Nt watches tells the
// application thread that the queue has something in it.
class Glue {
public:
    static Glue &instance();

    // Called by the integration on the application thread.
    int eventDescriptor() const { return m_pipe[0]; }
    void drainDescriptor();
    bool next(Event *event);
    // Waits until a native window exists, for at most timeoutMs; returns null if none appeared.
    ANativeWindow *waitForWindow(int timeoutMs);
    ANativeWindow *window();
    // Tells the UI thread that the application has stopped using the window it was told is going away.
    void windowReleased();
    int density() const; // dots per inch of the screen, or 160
    // The part of the window that is not under the system bars, in pixels; empty until the activity says.
    ARect contentRect();
    // The space that the system bars and cut-outs take at each edge, in pixels.
    ARect systemInsets();
    // The text on the clipboard of the system (empty if there is none, or the application may not read it), and
    // replacing it. Through JNI, on any thread.
    std::string clipboardText();
    void setClipboardText(const std::string &text);
    // Shows or hides the soft keyboard.
    void setSoftInputVisible(bool visible);
    // Whether the activity is the NtActivity of Nt, whose keyboard view delivers text and keys.
    bool hasNtActivity() const { return m_ntActivity; }
    void registerNtActivity();
    void setContentRect(const ARect &rect);
    ANativeActivity *activity() const { return m_activity; }
    void finish();

    // Called by the callbacks of the activity.
    void attach(ANativeActivity *activity);
    void post(const Event &event);
    void setWindow(ANativeWindow *window);
    void windowGoingAway();
    void setInputQueue(AInputQueue *queue);
    void configurationChanged();
    bool applicationRunning() const { return m_running; }
    void setApplicationRunning(bool running) { m_running = running; }

private:
    Glue() = default;

    ANativeActivity *m_activity = nullptr;
    AConfiguration *m_config = nullptr;
    int m_pipe[2] = {-1, -1};
    std::mutex m_mutex;
    std::condition_variable m_changed;
    std::deque<Event> m_events;
    ANativeWindow *m_window = nullptr;
    bool m_windowGone = true; // the application has let go of the previous window
    bool m_running = false;
    bool m_ntActivity = false;
    ARect m_content = {0, 0, 0, 0};
    AInputQueue *m_queue = nullptr;
};

} // namespace QtAndroid
