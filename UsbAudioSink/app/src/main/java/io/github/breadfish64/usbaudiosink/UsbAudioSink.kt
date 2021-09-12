package io.github.breadfish64.usbaudiosink

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.hardware.usb.UsbManager

class UsbAudioSink : Application() {
    companion object {
        private lateinit var context: Context
        lateinit var usbManager: UsbManager

        init {
            System.loadLibrary("native-lib")
        }

        fun getAppContext(): Context {
            return context
        }
    }

    override fun onCreate() {
        super.onCreate()
        context = applicationContext

        val mChannel =
            NotificationChannel("KeepAlive", "KeepAlive", NotificationManager.IMPORTANCE_LOW)
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(mChannel)

        usbManager = getSystemService(Service.USB_SERVICE) as UsbManager
    }
}