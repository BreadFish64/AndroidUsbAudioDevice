package io.github.breadfish64.usbaudiosink

import android.app.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.os.Parcelable
import android.os.Process
import java.io.Closeable
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

class UsbConnection(
    val fileDescriptor: ParcelFileDescriptor,
    val inputStream: FileInputStream,
    val outputStream: FileOutputStream) : Closeable {

    companion object {
        fun connect(accessory: UsbAccessory): UsbConnection? {
            val fd = UsbAudioSink.usbManager.openAccessory(accessory) ?: return null;
            val istream = FileInputStream(fd.fileDescriptor)
            val ostream = FileOutputStream(fd.fileDescriptor)
            return UsbConnection(fd, istream, ostream)
        }
    }

    override fun close() {
        try {
            inputStream.close()
            outputStream.close()
            fileDescriptor.close()
        } catch (e: IOException) {
        }
    }

    protected fun finalize() {
        close()
    }
}

class MainService : Service(), Runnable {
    private var mPermissionIntent: PendingIntent? = null
    private var mPermissionRequestPending = false

    var mAccessory: UsbAccessory? = null
    var usbConnection: UsbConnection? = null

    var thread: Thread? = null

    var SAMPLE_RATE = 0
    var SAMPLE_SIZE = 0
    var CHANNELS = 0

    var SAMPLE_CHANNELS = 0
    var SAMPLE_FORMAT = 0

    var isBeingDestroyed = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log("Service Started")

        val filter = IntentFilter(ACTION_USB_PERMISSION)
        filter.addAction(UsbManager.ACTION_USB_ACCESSORY_DETACHED)
        filter.addAction("ScreenLogger")
        registerReceiver(mUsbReceiver, filter)

        val accessory = intent?.getParcelableExtra<Parcelable>(UsbManager.EXTRA_ACCESSORY) as UsbAccessory?


        if (UsbAudioSink.usbManager.hasPermission(accessory)) {
            Log("Already have permissions, opening USB accessory")
            openAccessory(accessory)
        } else {
            mPermissionIntent =
                PendingIntent.getBroadcast(this, 0, Intent(ACTION_USB_PERMISSION), 0)

            UsbAudioSink.usbManager.requestPermission(accessory, mPermissionIntent)
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onCreate() {
        super.onCreate()

        val notification = Notification.Builder(applicationContext, "KeepAlive")
            .setContentTitle("Audio Sink").build()
        startForeground(1, notification)

        Log("Sanity Test")
    }

    override fun onDestroy() {
        unregisterReceiver(mUsbReceiver)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? {
        TODO("Not yet implemented")
    }

    private fun openAccessory(accessory: UsbAccessory?) {
        if (!UsbAudioSink.usbManager.hasPermission(accessory)) {
            Log("somehow we don't have accessory permissions")
        }
        usbConnection = accessory?.let { UsbConnection.connect(it) }
        if (usbConnection != null) {
            thread = Thread(null, this, "AccessoryController")
            thread!!.start()
        } else {
            Log("accessory open failed")
        }
    }

    private fun closeAccessory() {
        isBeingDestroyed
        thread?.join()
        usbConnection?.close()
    }

    /*
     * This receiver monitors for the event of a user granting permission to use
     * the attached accessory.  If the user has checked to always allow, this will
     * be generated following attachment without further user interaction.
     */
    private val mUsbReceiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val action = intent.action
            when {
                ACTION_USB_PERMISSION == action -> {
                    synchronized(this) {
                        val accessory =
                            intent.getParcelableExtra<Parcelable>(UsbManager.EXTRA_ACCESSORY) as UsbAccessory?
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            openAccessory(accessory)
                        } else {
                            Log("permission denied for accessory $accessory")
                        }
                        mPermissionRequestPending = false
                    }
                }
                UsbManager.ACTION_USB_ACCESSORY_DETACHED == action -> {
                    synchronized(this) {
                        try {
                            val accessory =
                                intent.getParcelableExtra<Parcelable>(UsbManager.EXTRA_ACCESSORY) as UsbAccessory?
                            if (accessory != null && accessory == mAccessory) {
                                closeAccessory()
                            }
                        } catch (e: Exception) {
                            Log(e.toString())
                        }
                        stopSelf()
                    }
                }
                "ScreenLogger" == action -> {
                    var log = intent.getStringExtra("log")
                    if (log == null) log = "No log attached to intent"
                    //usbConnection?.outputStream!!.write(log.toByteArray())
                }
            }
        }
    }

    /*
     * Runnable block that will poll the accessory data stream
     * for regular updates, posting each message it finds to a
     * Handler.  This is run on a spawned background thread.
     */
    override fun run() {
        try {
            Process.setThreadPriority(Process.THREAD_PRIORITY_MORE_FAVORABLE)

            val usbBuffer = ByteArray(AOA_BUF_SIZE)

            val sentinelSize = usbConnection!!.inputStream.read(usbBuffer)

            Log(usbBuffer.decodeToString(0, sentinelSize))
            usbConnection!!.outputStream.write("BreadFish64".toByteArray())

            usbConnection!!.inputStream.read(usbBuffer)
            val bb = ByteBuffer.wrap(usbBuffer)
            bb.order(ByteOrder.LITTLE_ENDIAN)
            SAMPLE_RATE = bb.int
            SAMPLE_SIZE = bb.int
            CHANNELS = bb.int

            usbConnection!!.outputStream.write("$SAMPLE_RATE Hz ${SAMPLE_SIZE * 8} bit $CHANNELS channel".toByteArray())

            SAMPLE_CHANNELS = when (CHANNELS) {
                1 -> AudioFormat.CHANNEL_OUT_MONO
                2 -> AudioFormat.CHANNEL_OUT_STEREO
                // TODO: support more than two channels?
                else -> AudioFormat.CHANNEL_OUT_STEREO
            }
            SAMPLE_FORMAT = when (SAMPLE_SIZE) {
                1 -> AudioFormat.ENCODING_PCM_8BIT
                2 -> AudioFormat.ENCODING_PCM_16BIT
                3 -> AudioFormat.ENCODING_PCM_FLOAT
                4 -> AudioFormat.ENCODING_PCM_FLOAT
                else -> AudioFormat.ENCODING_PCM_FLOAT
            }
            val bufferSamples = 4096

            // Not sure if this actually matters, since the audio content is arbitrary
            // I'm just assuming that if anything, USAGE_GAME discourages any extra audio processing/latency
            val attributes =
                AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build()
            val format =
                AudioFormat.Builder().setSampleRate(SAMPLE_RATE).setChannelMask(SAMPLE_CHANNELS)
                    .setEncoding(SAMPLE_FORMAT).build()
            // Now a sane person would use
            // val bufSize = AudioTrack.getMinBufferSize(SAMPLE_RATE, SAMPLE_CHANNELS, SAMPLE_FORMAT)
            // but I want even lower latency
            val bufSize = when (SAMPLE_FORMAT) {
                AudioFormat.ENCODING_PCM_8BIT -> 1
                AudioFormat.ENCODING_PCM_16BIT -> 2
                AudioFormat.ENCODING_PCM_FLOAT -> 4
                else -> 0
            } * CHANNELS * bufferSamples
            val audioTrack =
                AudioTrack.Builder().setAudioAttributes(attributes).setAudioFormat(format)
                    .setBufferSizeInBytes(bufSize).setTransferMode(AudioTrack.MODE_STREAM)
                    .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY).build()

            val floatBuffer = FloatArray(usbBuffer.size / SAMPLE_SIZE)

            fun pushChunk(): Boolean {
                val packetSize = usbConnection!!.inputStream.read(usbBuffer)

                var byteIdx = 0

                val samples = (packetSize - byteIdx) / SAMPLE_SIZE
                val sampleBytes = samples * SAMPLE_SIZE
                when (SAMPLE_SIZE) {
                    1, 2, 4 -> {
                        audioTrack.write(usbBuffer, byteIdx, sampleBytes, AudioTrack.WRITE_BLOCKING)
                    }
                    3 -> {
                        val start = System.nanoTime()
                        I24toF32(floatBuffer, 0, usbBuffer, byteIdx.toLong(), samples.toLong())
                        val time = System.nanoTime() - start
                        audioTrack.write(floatBuffer, 0, samples, AudioTrack.WRITE_BLOCKING)
                        //usbConnection!!.outputStream.write("conversion took $time ns".toByteArray())
                    }
                }
                return true
            }
            audioTrack.play()
            usbConnection!!.outputStream.write("BreadFish64".toByteArray())
            usbConnection!!.outputStream.write("Android buffer size: $bufSize".toByteArray())
            var underruns = audioTrack.underrunCount
            while (!isBeingDestroyed) {
                if (!pushChunk()) return
                if (audioTrack.underrunCount > underruns) {
                    underruns = audioTrack.underrunCount
                    usbConnection!!.outputStream.write("underrun $underruns occurred".toByteArray())
                }
            }
        } catch (e: Exception) {
            Log(e.toString())
            // Log(e.stackTraceToString())
        }
    }

    companion object {
        private const val ACTION_USB_PERMISSION =
            "com.examples.accessory.controller.action.USB_PERMISSION"
        const val AOA_BUF_SIZE = 16384
    }
}