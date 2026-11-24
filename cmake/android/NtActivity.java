// MIT License - see LICENSE
//
// The activity of an Nt application on Android: a NativeActivity with a view that the on-screen keyboard can type
// into. NativeActivity's own view is not a text editor, so the system will not show a keyboard for it. The text and
// the keys that the keyboard produces go to the native code of Nt GUI.
package org.nt;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

public class NtActivity extends NativeActivity {
    // Registered by Nt GUI when the application starts.
    private static native void nativeText(String text);

    private static native void nativeKey(int action, int keyCode, int metaState);

    // The system bars or the keyboard changed the space that is left for the window.
    private static native void nativeInsetsChanged();

    private InputView inputView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        inputView = new InputView(this);
        addContentView(inputView, new FrameLayout.LayoutParams(1, 1));
        getWindow().getDecorView().setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                nativeInsetsChanged();
                return view.onApplyWindowInsets(insets);
            }
        });
    }

    // Shows or hides the keyboard; called from the application thread through JNI.
    public void showNtInput(final boolean show) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm == null)
                    return;
                if (show) {
                    inputView.requestFocus();
                    imm.showSoftInput(inputView, 0);
                } else {
                    imm.hideSoftInputFromWindow(inputView.getWindowToken(), 0);
                }
            }
        });
    }

    private static class InputView extends View {
        InputView(Context context) {
            super(context);
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo info) {
            // Plain text, without suggestions or composing: the keyboard commits each character at once, which is
            // what a widget that has no input method events can take.
            info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                    | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
            info.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                    | EditorInfo.IME_ACTION_NONE;
            return new BaseInputConnection(this, false) {
                @Override
                public boolean commitText(CharSequence text, int newCursorPosition) {
                    nativeText(text.toString());
                    return true;
                }

                @Override
                public boolean setComposingText(CharSequence text, int newCursorPosition) {
                    nativeText(text.toString());
                    return true;
                }

                @Override
                public boolean finishComposingText() {
                    return true;
                }

                @Override
                public boolean deleteSurroundingText(int before, int after) {
                    for (int i = 0; i < before; ++i) {
                        nativeKey(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0);
                        nativeKey(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0);
                    }
                    for (int i = 0; i < after; ++i) {
                        nativeKey(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_FORWARD_DEL, 0);
                        nativeKey(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_FORWARD_DEL, 0);
                    }
                    return true;
                }

                @Override
                public boolean sendKeyEvent(KeyEvent event) {
                    nativeKey(event.getAction(), event.getKeyCode(), event.getMetaState());
                    return true;
                }
            };
        }
    }
}
