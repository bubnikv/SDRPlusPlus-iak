package org.sdrpp.sdrpp;

import android.app.NativeActivity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.hardware.usb.*;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.Manifest;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.util.Log;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.content.res.Configuration;

import androidx.core.app.ActivityCompat;
import androidx.core.content.IntentCompat;

import androidx.core.content.PermissionChecker;

import java.util.concurrent.LinkedBlockingQueue;
import java.io.*;

private const val ACTION_USB_PERMISSION = "org.sdrpp.sdrpp.USB_PERMISSION";

private val usbReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val activity = context as? MainActivity ?: return
        when (intent.action) {
            ACTION_USB_PERMISSION -> {
                synchronized(this) {
                    activity.SDR_device = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        activity.SDR_conn = activity.usbManager?.openDevice(activity.SDR_device)
                        if (activity.SDR_conn != null && activity.SDR_device != null) {
                            activity.SDR_VID = activity.SDR_device!!.vendorId
                            activity.SDR_PID = activity.SDR_device!!.productId
                            activity.SDR_FD = activity.SDR_conn!!.fileDescriptor
                        }
                        activity.notifyUsbHotplugChanged()
                    }

                    activity.hideSystemBars()
                }
            }
            UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                val device = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                if (device != null) {
                    val manager = activity.usbManager
                    if (manager != null && manager.hasPermission(device)) {
                        activity.notifyUsbHotplugChanged()
                    }
                    else {
                        activity.requestUsbPermission(device)
                    }
                }
            }
            UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                val device = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                if (device != null && activity.SDR_device?.deviceId == device.deviceId) {
                    activity.SDR_conn?.close()
                    activity.SDR_conn = null
                    activity.SDR_device = null
                    activity.SDR_VID = -1
                    activity.SDR_PID = -1
                    activity.SDR_FD = -1
                }
                if (device != null) {
                    activity.releaseRetainedUsbConnection(device.deviceName)
                    activity.notifyUsbHotplugChanged()
                }
            }
        }
    }
}
class MainActivity : NativeActivity() {
    private val TAG : String = "SDR++";
    public var usbManager : UsbManager? = null;
    public var SDR_device : UsbDevice? = null;
    public var SDR_conn : UsbDeviceConnection? = null;
    public var SDR_VID : Int = -1;
    public var SDR_PID : Int = -1;
    public var SDR_FD : Int = -1;

    // Sleep timer
    private lateinit var sleepTimer: SleepTimerManager;
    private var wakeLock: PowerManager.WakeLock? = null;
    private var usbPermissionIntent: PendingIntent? = null
    private var usbReceiverRegistered = false

    companion object {
        private const val QMX_USB_VID = 0x0483
        private const val QMX_USB_PID = 0xA34C
        private val openUsbConnections = HashMap<Int, UsbDeviceConnection>()
        private val openUsbDeviceNames = HashMap<String, Int>()

        init {
            System.loadLibrary("sdrpp_core")
        }

        private fun isQmxUsbDevice(dev: UsbDevice?): Boolean {
            return dev != null && dev.vendorId == QMX_USB_VID && dev.productId == QMX_USB_PID
        }

        private fun isUsbAudioDevice(device: AudioDeviceInfo): Boolean {
            return device.type == AudioDeviceInfo.TYPE_USB_DEVICE ||
                device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
                device.type == AudioDeviceInfo.TYPE_USB_ACCESSORY
        }

        private fun containsQmxMarker(value: CharSequence?): Boolean {
            if (value == null) {
                return false
            }
            val text = value.toString().uppercase()
            return text.contains("QMX") || text.contains("QDX")
        }

        private fun isLikelyQmxAudioDevice(context: Context, device: AudioDeviceInfo, directionFlag: Int): Boolean {
            if (!isUsbAudioDevice(device)) {
                return false
            }
            val addressMatches = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                containsQmxMarker(device.address)
            }
            else {
                false
            }
            if (containsQmxMarker(device.productName) || addressMatches) {
                return true
            }

            val usbManager = context.getSystemService(Context.USB_SERVICE) as? UsbManager ?: return false
            val qmxDevices = usbManager.deviceList.values.filter { isQmxUsbDevice(it) }
            if (qmxDevices.isEmpty()) {
                return false
            }

            for (usbDevice in qmxDevices) {
                if (containsQmxMarker(usbDevice.productName) ||
                    containsQmxMarker(usbDevice.manufacturerName) ||
                    containsQmxMarker(usbDevice.deviceName) ||
                    device.productName.toString().equals(usbDevice.productName ?: "", ignoreCase = true)
                ) {
                    return true
                }
            }

            val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as? AudioManager ?: return false
            val usbAudioCount = audioManager.getDevices(directionFlag).count { isUsbAudioDevice(it) }
            return usbAudioCount == 1
        }

        private fun outputRank(device: AudioDeviceInfo): Int {
            return when (device.type) {
                AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
                AudioDeviceInfo.TYPE_WIRED_HEADSET -> 0
                AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
                AudioDeviceInfo.TYPE_BLE_HEADSET,
                AudioDeviceInfo.TYPE_BLE_SPEAKER,
                AudioDeviceInfo.TYPE_BLE_BROADCAST -> 1
                AudioDeviceInfo.TYPE_BUILTIN_SPEAKER,
                AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE -> 2
                AudioDeviceInfo.TYPE_BUILTIN_EARPIECE -> 3
                AudioDeviceInfo.TYPE_HDMI,
                AudioDeviceInfo.TYPE_HDMI_ARC,
                AudioDeviceInfo.TYPE_HDMI_EARC,
                AudioDeviceInfo.TYPE_LINE_ANALOG,
                AudioDeviceInfo.TYPE_LINE_DIGITAL,
                AudioDeviceInfo.TYPE_DOCK -> 4
                else -> if (isUsbAudioDevice(device)) 6 else 5
            }
        }

        private fun inputRank(device: AudioDeviceInfo): Int {
            return when (device.type) {
                AudioDeviceInfo.TYPE_WIRED_HEADSET -> 0
                AudioDeviceInfo.TYPE_BLUETOOTH_SCO,
                AudioDeviceInfo.TYPE_BLE_HEADSET -> 1
                AudioDeviceInfo.TYPE_BUILTIN_MIC,
                AudioDeviceInfo.TYPE_FM_TUNER,
                AudioDeviceInfo.TYPE_TELEPHONY -> 2
                else -> if (isUsbAudioDevice(device)) 4 else 3
            }
        }

        private fun getPreferredAudioDeviceId(context: Context, directionFlag: Int): Int {
            val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as? AudioManager ?: return 0
            val devices = audioManager.getDevices(directionFlag)
            if (devices.isEmpty()) {
                return 0
            }

            val candidates = devices.filterNot { isLikelyQmxAudioDevice(context, it, directionFlag) }
            if (candidates.isEmpty()) {
                return 0
            }
            val ranked = if (directionFlag == AudioManager.GET_DEVICES_OUTPUTS) {
                candidates.minByOrNull(::outputRank)
            }
            else {
                candidates.minByOrNull(::inputRank)
            }
            return ranked?.id ?: 0
        }

        private fun getRetainedUsbConnectionFd(device: UsbDevice): Int? {
            synchronized(openUsbConnections) {
                val existingFd = openUsbDeviceNames[device.deviceName] ?: return null
                if (openUsbConnections.containsKey(existingFd)) {
                    return existingFd
                }
                openUsbDeviceNames.remove(device.deviceName)
                return null
            }
        }

        private fun retainUsbConnection(device: UsbDevice, conn: UsbDeviceConnection): Int {
            val fd = conn.fileDescriptor
            if (fd < 0) {
                conn.close()
                return -1
            }

            synchronized(openUsbConnections) {
                val oldFdForDevice = openUsbDeviceNames.remove(device.deviceName)
                if (oldFdForDevice != null) {
                    openUsbConnections.remove(oldFdForDevice)?.close()
                }

                val staleMappings = openUsbDeviceNames
                    .filterValues { it == fd }
                    .keys
                    .toList()
                for (deviceName in staleMappings) {
                    openUsbDeviceNames.remove(deviceName)
                }

                openUsbConnections.remove(fd)?.close()
                openUsbConnections[fd] = conn
                openUsbDeviceNames[device.deviceName] = fd
            }

            return fd
        }

        private fun closeRetainedUsbConnection(fd: Int) {
            synchronized(openUsbConnections) {
                openUsbConnections.remove(fd)?.close()
                val staleMappings = openUsbDeviceNames
                    .filterValues { it == fd }
                    .keys
                    .toList()
                for (deviceName in staleMappings) {
                    openUsbDeviceNames.remove(deviceName)
                }
            }
        }

        private fun closeRetainedUsbConnection(deviceName: String) {
            synchronized(openUsbConnections) {
                val fd = openUsbDeviceNames.remove(deviceName) ?: return
                openUsbConnections.remove(fd)?.close()
            }
        }

        private fun clearRetainedUsbConnections() {
            synchronized(openUsbConnections) {
                openUsbConnections.values.forEach { it.close() }
                openUsbConnections.clear()
                openUsbDeviceNames.clear()
            }
        }

        /**
         * Returns the file descriptor for a USB device matching the given VID and PID.
         * If multiple devices match, returns the first one found.
         * Returns -1 if not found or permission denied.
         * This can be called from JNI/native code.
         */
        @JvmStatic
        fun getDeviceFDByVidPid(context: Context, vid: Int, pid: Int): Int {
            val usbManager = context.getSystemService(Context.USB_SERVICE) as? UsbManager ?: return -1
            val devList = usbManager.deviceList
            for ((_, dev) in devList) {
                if (dev.vendorId == vid && dev.productId == pid) {
                    if (!usbManager.hasPermission(dev)) {
                        (context as? MainActivity)?.requestUsbPermission(dev)
                        return -1
                    }
                    val existingFd = getRetainedUsbConnectionFd(dev)
                    if (existingFd != null) {
                        return existingFd
                    }
                    val conn = usbManager.openDevice(dev)
                    if (conn != null) {
                        return retainUsbConnection(dev, conn)
                    }
                }
            }
            return -1
        }

        @JvmStatic
        fun getOpenUsbDeviceHandleByVidPid(context: Context, vid: Int, pid: Int): String? {
            val usbManager = context.getSystemService(Context.USB_SERVICE) as? UsbManager ?: return null
            val devList = usbManager.deviceList
            for ((_, dev) in devList) {
                if (dev.vendorId == vid && dev.productId == pid) {
                    if (!usbManager.hasPermission(dev)) {
                        (context as? MainActivity)?.requestUsbPermission(dev)
                        return null
                    }
                    val existingFd = getRetainedUsbConnectionFd(dev)
                    if (existingFd != null) {
                        return "$existingFd|${dev.deviceName}"
                    }
                    val conn = usbManager.openDevice(dev) ?: return null
                    val fd = retainUsbConnection(dev, conn)
                    if (fd < 0) {
                        return null
                    }
                    return "$fd|${dev.deviceName}"
                }
            }
            return null
        }

        @JvmStatic
        fun releaseOpenUsbDeviceHandle(fd: Int) {
            closeRetainedUsbConnection(fd)
        }

        @JvmStatic
        fun getPreferredAudioOutputDeviceId(context: Context): Int {
            return getPreferredAudioDeviceId(context, AudioManager.GET_DEVICES_OUTPUTS)
        }

        @JvmStatic
        fun getPreferredAudioInputDeviceId(context: Context): Int {
            return getPreferredAudioDeviceId(context, AudioManager.GET_DEVICES_INPUTS)
        }
    }

    fun checkAndAsk(permission: String) {
        if (PermissionChecker.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(permission), 1);
        }
    }

    private fun registerUsbReceiver() {
        if (usbReceiverRegistered) {
            return
        }
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(usbReceiver, filter)
        }
        usbReceiverRegistered = true
    }

    private fun unregisterUsbReceiver() {
        if (!usbReceiverRegistered) {
            return
        }
        try {
            unregisterReceiver(usbReceiver)
        }
        catch (_: IllegalArgumentException) {
        }
        usbReceiverRegistered = false
    }

    fun requestUsbPermission(device: UsbDevice) {
        val manager = usbManager ?: return
        if (manager.hasPermission(device)) {
            return
        }
        val permissionIntent = usbPermissionIntent ?: return
        manager.requestPermission(device, permissionIntent)
    }

    private fun requestUsbPermissionsForConnectedDevices() {
        val manager = usbManager ?: return
        for ((_, dev) in manager.deviceList) {
            if (manager.hasPermission(dev)) {
                notifyUsbHotplugChanged()
            }
            else {
                requestUsbPermission(dev)
            }
        }
    }

    fun releaseRetainedUsbConnection(deviceName: String) {
        Companion.closeRetainedUsbConnection(deviceName)
    }

    private external fun notifyUsbHotplugChangedNative()

    fun notifyUsbHotplugChanged() {
        notifyUsbHotplugChangedNative()
    }

    private fun handleUsbAttachIntent(intent: Intent?) {
        if (intent?.action != UsbManager.ACTION_USB_DEVICE_ATTACHED) {
            return
        }
        val device = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
        if (device != null) {
            val manager = usbManager
            if (manager != null && manager.hasPermission(device)) {
                notifyUsbHotplugChanged()
            }
            else {
                requestUsbPermission(device)
            }
        }
    }

    public fun hideSystemBars() {
        val decorView = getWindow().getDecorView();
        val uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        decorView.setSystemUiVisibility(uiOptions);
    }

    public override fun onCreate(savedInstanceState: Bundle?) {
        // Force landscape orientation on phone-sized devices (smaller than large/tablet)
        val screenLayout = resources.configuration.screenLayout and Configuration.SCREENLAYOUT_SIZE_MASK
        if (screenLayout < Configuration.SCREENLAYOUT_SIZE_LARGE) {
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }

        // Initialize sleep timer
        sleepTimer = SleepTimerManager(this);

        // Hide bars
        hideSystemBars();

        // Ask for required permissions, without these the app cannot run.
        checkAndAsk(Manifest.permission.WRITE_EXTERNAL_STORAGE);
        checkAndAsk(Manifest.permission.READ_EXTERNAL_STORAGE);
//        checkAndAsk(Manifest.permission.RECORD_AUDIO);

        // TODO: Have the main code wait until these two permissions are available

        // Register events
        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager;
        val permissionRequestIntent = Intent(ACTION_USB_PERMISSION).setPackage(packageName)
        val pendingIntentFlags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
        } else {
            PendingIntent.FLAG_UPDATE_CURRENT
        }
        usbPermissionIntent = PendingIntent.getBroadcast(this, 0, permissionRequestIntent, pendingIntentFlags)
        registerUsbReceiver()
        handleUsbAttachIntent(intent)

        // Get permission for all USB devices
        requestUsbPermissionsForConnectedDevices()

        // Ask for internet permission
        checkAndAsk(Manifest.permission.INTERNET);

        super.onCreate(savedInstanceState)
    }

    public override fun onResume() {
        // Hide bars again
        hideSystemBars();
        super.onResume();
    }

    public override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleUsbAttachIntent(intent)
    }

    public override fun onDestroy() {
        sleepTimer.stop();
        unregisterUsbReceiver()
        SDR_conn?.close()
        SDR_conn = null
        Companion.clearRetainedUsbConnections()
        super.onDestroy();
    }

    // ── Sleep timer API ──────────────────────────────────────────────

    /**
     * Start the sleep timer.  Callable from native code via JNI.
     */
    fun startSleepTimer() {
        runOnUiThread { sleepTimer.start() }
    }

    /**
     * Stop the sleep timer and restore normal behavior.
     * Callable from native code via JNI.
     */
    fun stopSleepTimer() {
        runOnUiThread { sleepTimer.stop() }
    }

    /** Set screen brightness. -1f = system default, 0f = off, 0.01f = dim. */
    fun applySleepBrightness(brightness: Float) {
        runOnUiThread {
            val lp = window.attributes
            lp.screenBrightness = brightness
            window.attributes = lp
        }
    }

    /** Add FLAG_KEEP_SCREEN_ON and acquire a WakeLock to prevent the system from suspending. */
    fun applyKeepScreenOn() {
        runOnUiThread {
            Log.i("SleepTimer", "applyKeepScreenOn: setting FLAG_KEEP_SCREEN_ON and acquiring WakeLock")
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            if (wakeLock == null) {
                val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
                wakeLock = pm.newWakeLock(
                    PowerManager.PARTIAL_WAKE_LOCK,
                    "SDRPlusPlus:SleepTimer"
                )
            }
            if (wakeLock?.isHeld == false) {
                wakeLock?.acquire(60L * 60 * 1000)  // 1 hour timeout as safety net
                Log.i("SleepTimer", "WakeLock acquired")
            }
        }
    }

    /** Remove FLAG_KEEP_SCREEN_ON and release the WakeLock — system idle timer resumes. */
    fun clearKeepScreenOn() {
        runOnUiThread {
            Log.i("SleepTimer", "clearKeepScreenOn: clearing FLAG_KEEP_SCREEN_ON and releasing WakeLock")
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            if (wakeLock?.isHeld == true) {
                wakeLock?.release()
                Log.i("SleepTimer", "WakeLock released")
            }
        }
    }

    /** Tell the native render loop to pause or resume. Zero-cost JNI: directly sets C++ flag. */
    fun setSleepRenderPaused(paused: Boolean) {
        nativeSetSleepRenderPaused(paused)
    }

    /** Mark the screen as dimmed (DIM or DARK phase). Guards touch/resume wake in C++. */
    fun setSleepScreenDimmed(dimmed: Boolean) {
        nativeSetSleepScreenDimmed(dimmed)
    }

    /** Native method that directly writes backend::sleepRenderPaused in C++. */
    private external fun nativeSetSleepRenderPaused(paused: Boolean)

    /** Native method that directly writes backend::sleepScreenDimmed in C++. */
    private external fun nativeSetSleepScreenDimmed(dimmed: Boolean)

    /**
     * Find the SurfaceView used by NativeActivity by walking the view hierarchy.
     */
    private fun findSurfaceView(view: View): SurfaceView? {
        if (view is SurfaceView) return view
        if (view is ViewGroup) {
            for (i in 0 until view.childCount) {
                val result = findSurfaceView(view.getChildAt(i))
                if (result != null) return result
            }
        }
        return null
    }

    /**
     * Hint the display to drop to 1 Hz refresh rate (LTPO panels).
     * Only effective on API 30+ (Android 11+); no-op on older devices.
     */
    fun setLowFrameRate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            runOnUiThread {
                try {
                    val sv = findSurfaceView(window.decorView)
                    val surface = sv?.holder?.surface
                    if (surface != null && surface.isValid) {
                        surface.setFrameRate(
                            1.0f,
                            Surface.FRAME_RATE_COMPATIBILITY_DEFAULT
                        )
                        Log.i("SleepTimer", "setFrameRate(1.0) applied")
                    } else {
                        Log.w("SleepTimer", "setFrameRate: no valid Surface found")
                    }
                } catch (e: Exception) {
                    Log.w("SleepTimer", "setFrameRate failed: ${e.message}")
                }
            }
        }
    }

    /**
     * Restore the default frame rate (remove the 1 Hz hint).
     * Only effective on API 30+ (Android 11+); no-op on older devices.
     */
    fun restoreFrameRate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            runOnUiThread {
                try {
                    val sv = findSurfaceView(window.decorView)
                    val surface = sv?.holder?.surface
                    if (surface != null && surface.isValid) {
                        surface.setFrameRate(
                            0.0f,
                            Surface.FRAME_RATE_COMPATIBILITY_DEFAULT
                        )
                        Log.i("SleepTimer", "setFrameRate(0.0) – restored default")
                    }
                } catch (e: Exception) {
                    Log.w("SleepTimer", "restoreFrameRate failed: ${e.message}")
                }
            }
        }
    }

    /**
     * Reset the sleep timer to Active phase.
     * Called from native code on touch-to-wake or phone resume.
     */
    fun resetSleepToActive() {
        runOnUiThread { sleepTimer.resetToActive() }
    }

    fun showSoftInput() {
        val inputMethodManager = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager;
        inputMethodManager.showSoftInput(window.decorView, 0);
    }

    fun hideSoftInput() {
        val inputMethodManager = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager;
        inputMethodManager.hideSoftInputFromWindow(window.decorView.windowToken, 0);
        hideSystemBars();
    }

    // Queue for the Unicode characters to be polled from native code (via pollUnicodeChar())
    private var unicodeCharacterQueue: LinkedBlockingQueue<Int> = LinkedBlockingQueue()

    // We assume dispatchKeyEvent() of the NativeActivity is actually called for every
    // KeyEvent and not consumed by any View before it reaches here
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN) {
            unicodeCharacterQueue.offer(event.getUnicodeChar(event.metaState))
        }
        return super.dispatchKeyEvent(event)
    }

    fun pollUnicodeChar(): Int {
        return unicodeCharacterQueue.poll() ?: 0
    }

    public fun createIfDoesntExist(path: String) {
        // This is a directory, create it in the filesystem
        var folder = File(path);
        var success = true;
        if (!folder.exists()) {
            success = folder.mkdirs();
        }
        if (!success) {
            Log.e(TAG, "Could not create folder with path " + path);
        }
    }

    public fun extractDir(aman: AssetManager, local: String, rsrc: String): Int {
        val flist = aman.list(rsrc) ?: return 0;
        var ecount = 0;
        for (fp in flist) {
            val lpath = local + "/" + fp;
            val rpath = rsrc + "/" + fp;

            Log.w(TAG, "Extracting '" + rpath + "' to '" + lpath + "'");

            // Create local path if non-existent
            createIfDoesntExist(local);
            
            // Create if directory
            val ext = extractDir(aman, lpath, rpath);

            // Extract if file
            if (ext == 0) {
                // This is a file, extract it
                val _os = FileOutputStream(lpath);
                val _is = aman.open(rpath);
                val ilen = _is.available();
                var fbuf = ByteArray(ilen);
                _is.read(fbuf, 0, ilen);
                _os.write(fbuf);
                _os.close();
                _is.close();
            }

            ecount++;
        }
        return ecount;
    }

    public fun getAppDir(): String {
        val fdir = getFilesDir().getAbsolutePath();

        // Extract all resources to the app directory
        val aman = getAssets();
        extractDir(aman, fdir + "/res", "res");
        createIfDoesntExist(fdir + "/modules");

        return fdir;
    }
}
