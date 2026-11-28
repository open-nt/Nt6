// MIT License - see LICENSE
//
// A service declared through nt_android_add_apk(MANIFEST_APPLICATION ...): it only has to compile against android.jar
// and be found in the manifest.
package org.nt.smoke;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class SmokeService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
