package org.sdrpp.sdrpp

import android.os.Handler
import android.os.Looper
import android.util.Log

/**
 * Manages a progressive sleep timer with three phases:
 *
 *   Active  (0–3 min)  : Screen at system default brightness, rendering active.
 *   Dim     (3–8 min)  : Screen dimmed to near-minimum, rendering active.
 *   Dark    (8–60 min) : Screen brightness 0 (appears off), rendering paused.
 *   End     (60 min)   : FLAG_KEEP_SCREEN_ON cleared; system suspends normally.
 *
 * Touch-to-wake: any touch during Dim or Dark resets the timer to Active.
 */
class SleepTimerManager(private val activity: MainActivity) {

    companion object {
        private const val TAG = "SleepTimer"

        // Phase durations in milliseconds
        private const val ACTIVE_DURATION_MS  =  3L * 60 * 1000   //  3 minutes
        private const val DIM_DURATION_MS     =  5L * 60 * 1000   //  5 minutes (3+5 = 8 min total)
        private const val DARK_DURATION_MS    = 52L * 60 * 1000   // 52 minutes (8+52 = 60 min total)
    }

    enum class Phase { IDLE, ACTIVE, DIM, DARK }

    var currentPhase: Phase = Phase.IDLE
        private set

    private val handler = Handler(Looper.getMainLooper())

    private val enterDimRunnable = Runnable {
        Log.i(TAG, "Entering DIM phase")
        currentPhase = Phase.DIM
        activity.applySleepBrightness(0.01f)
        activity.setSleepScreenDimmed(true)
    }

    private val enterDarkRunnable = Runnable {
        Log.i(TAG, "Entering DARK phase")
        currentPhase = Phase.DARK
        activity.applySleepBrightness(0f)
        activity.setSleepRenderPaused(true)
        activity.setLowFrameRate()
    }

    private val enterEndRunnable = Runnable {
        Log.i(TAG, "Entering END phase – releasing keep-screen-on")
        currentPhase = Phase.IDLE
        // Don't restore brightness – leave at 0 so there's no bright flash.
        // Just clear keep-screen-on; the system will suspend soon.
        activity.clearKeepScreenOn()
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)  // let the render loop resume normally
    }

    /**
     * Start (or restart) the sleep timer from the Active phase.
     */
    fun start() {
        Log.i(TAG, "Starting sleep timer")
        cancelAllCallbacks()

        currentPhase = Phase.ACTIVE
        activity.applyKeepScreenOn()
        activity.applySleepBrightness(-1f)          // system default
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)
        activity.restoreFrameRate()

        handler.postDelayed(enterDimRunnable,  ACTIVE_DURATION_MS)
        handler.postDelayed(enterDarkRunnable, ACTIVE_DURATION_MS + DIM_DURATION_MS)
        handler.postDelayed(enterEndRunnable,  ACTIVE_DURATION_MS + DIM_DURATION_MS + DARK_DURATION_MS)
    }

    /**
     * Stop the sleep timer and restore everything to normal.
     */
    fun stop() {
        Log.i(TAG, "Stopping sleep timer")
        cancelAllCallbacks()
        currentPhase = Phase.IDLE
        activity.applySleepBrightness(-1f)
        activity.clearKeepScreenOn()
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)
    }

    /**
     * Called on touch or phone wake during Dim or Dark phase.
     * Resets the timer back to the Active phase.
     */
    fun resetToActive() {
        if (currentPhase == Phase.DIM || currentPhase == Phase.DARK || currentPhase == Phase.ACTIVE) {
            Log.i(TAG, "Resetting from ${currentPhase} phase to ACTIVE")
            start()   // restart from scratch
        }
    }

    val isRunning: Boolean
        get() = currentPhase != Phase.IDLE

    private fun cancelAllCallbacks() {
        handler.removeCallbacks(enterDimRunnable)
        handler.removeCallbacks(enterDarkRunnable)
        handler.removeCallbacks(enterEndRunnable)
    }
}
