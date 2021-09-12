package io.github.breadfish64.usbaudiosink

// https://github.com/follower/android-background-service-usb-accessory

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.util.Log

class LaunchServiceActivity : Activity() {
    /** Called when the activity is first created.  */
    private val TAG = "StartServiceActivity"
    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(TAG, "onCreate entered")
        val intent = Intent(this, MainService::class.java)
        intent.fillIn(
            getIntent(),
            0
        ) // TODO: Find better way to get extras for `UsbManager.getAccessory()` use?
        val service = startForegroundService(intent)
        Log("Service: $service")

        // See:
        //
        //    <http://permalink.gmane.org/gmane.comp.handhelds.android.devel/154481> &
        //    <http://stackoverflow.com/questions/5567312/android-how-to-execute-main-fucntionality-of-project-only-by-clicking-on-the-ic/5567514#5567514>
        //
        // for combination of `Theme.NoDisplay` and `finish()` in `onCreate()`/`onResume()`.
        //
        finish()
        Log.d(TAG, "onCreate exited")
    }
}