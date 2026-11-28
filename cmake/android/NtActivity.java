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

    // The answer to requestNtPermissions(); registered by Nt Core the first time the application asks for a permission.
    private static native void nativePermissionResult(int requestCode, String[] permissions, int[] grantResults);

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

    // Asks the user for Android permissions (QCoreApplication::requestPermission()); called from the application
    // thread through JNI. The answer comes to onRequestPermissionsResult().
    public void requestNtPermissions(final String[] permissions, final int requestCode) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                requestPermissions(permissions, requestCode);
            }
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        try {
            nativePermissionResult(requestCode, permissions, grantResults);
        } catch (UnsatisfiedLinkError e) {
            // A request the application made itself, before Nt Core asked for anything: not Nt's to answer.
        }
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

    // What the widget with the focus takes, as Qt::InputMethodHints; called from the application thread through JNI.
    public void setNtInputHints(final int hints) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (inputView == null)
                    return;
                boolean other = inputTypeFor(hints) != inputTypeFor(inputView.hints);
                inputView.hints = hints;
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                // A keyboard that is up asks again, and changes its keys.
                if (other && imm != null)
                    imm.restartInput(inputView);
            }
        });
    }

    // The kind of keyboard for the hints, the values of Qt::InputMethodHint, mapped as Qt does.
    private static int inputTypeFor(int hints) {
        final int hiddenText = 0x1;
        final int preferNumbers = 0x8;
        final int date = 0x80;
        final int time = 0x100;
        final int digitsOnly = 0x10000;
        final int formattedNumbersOnly = 0x20000;
        final int dialableCharactersOnly = 0x100000;
        final int emailCharactersOnly = 0x200000;
        final int urlCharactersOnly = 0x400000;
        final boolean hidden = (hints & hiddenText) != 0;
        if ((hints & dialableCharactersOnly) != 0)
            return InputType.TYPE_CLASS_PHONE;
        if ((hints & (preferNumbers | digitsOnly | formattedNumbersOnly)) != 0) {
            int type = InputType.TYPE_CLASS_NUMBER;
            if ((hints & formattedNumbersOnly) != 0)
                type |= InputType.TYPE_NUMBER_FLAG_SIGNED | InputType.TYPE_NUMBER_FLAG_DECIMAL;
            return hidden ? type | InputType.TYPE_NUMBER_VARIATION_PASSWORD : type;
        }
        if ((hints & (date | time)) != 0)
            return InputType.TYPE_CLASS_DATETIME
                    | ((hints & date) == 0 ? InputType.TYPE_DATETIME_VARIATION_TIME
                            : (hints & time) == 0 ? InputType.TYPE_DATETIME_VARIATION_DATE : 0);
        // Text, without suggestions or composing: the keyboard commits each character at once, which is what a widget
        // that has no input method events can take.
        final int text = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        if (hidden)
            return text | InputType.TYPE_TEXT_VARIATION_PASSWORD;
        if ((hints & emailCharactersOnly) != 0)
            return text | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        if ((hints & urlCharactersOnly) != 0)
            return text | InputType.TYPE_TEXT_VARIATION_URI;
        return text | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
    }

    private static class InputView extends View {
        int hints; // read and written on the thread of the UI

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
            info.inputType = inputTypeFor(hints);
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
