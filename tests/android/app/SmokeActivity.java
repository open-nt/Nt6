// MIT License - see LICENSE
//
// An application's own activity: it extends org.nt.NtActivity, so the window, input and keyboard of Nt stay, and adds
// its own start-up (here it starts SmokeService). Checked by the android workflow: built into classes.dex and named
// in the manifest by nt_android_add_apk(ACTIVITY ...).
package org.nt.smoke;

import android.content.Intent;
import android.os.Bundle;

import org.nt.NtActivity;

public class SmokeActivity extends NtActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        startService(new Intent(this, SmokeService.class));
    }
}
