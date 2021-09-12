package io.github.breadfish64.usbaudiosink

import android.content.Intent
import androidx.localbroadcastmanager.content.LocalBroadcastManager

fun Log(msg: String) {
    val intent = Intent("ScreenLogger")
    intent.putExtra("log", msg + '\n')
    LocalBroadcastManager.getInstance(UsbAudioSink.getAppContext()).sendBroadcast(intent)
    android.util.Log.e("ScreenLogger", msg)
}