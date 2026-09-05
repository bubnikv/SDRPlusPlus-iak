package org.ok1iak.sdriak

import android.os.Handler
import android.os.Looper
import android.util.Log

/**
 * Manages screen keep-alive behaviour in four modes:
 *
 *   DISABLED      : No override; system screen-timeout applies normally.
 *   KEEP_ALIVE    : FLAG_KEEP_SCREEN_ON held; screen stays on at full brightness.
 *   DIM_SCREEN    : Keep-alive + dims after [dimAfterMs]; touch restores brightness.
 *   DIM_AND_BLANK : Keep-alive + dims after [dimAfterMs] + blanks after [darkAfterMs];
 *                   touch restores brightness. After a further 52 min in the dark the
 *                   keep-screen-on flag is released and the system suspends normally.
 *
 * Touch-to-wake: any touch during Dim or Dark resets the timer to Active.
 */
class SleepTimerManager(private val activity: MainActivity) {

    companion object {
        private const val TAG = "SleepTimer"

        // How long the screen stays dark (DIM_AND_BLANK only) before keep-screen-on is released.
        private const val DARK_DURATION_MS = 60L * 60 * 1000  // 60 min max without user interaction before system can suspend
    }

    // ── Configurable thresholds (total milliseconds from timer start) ────────────
    // Defaults: dim after 3 min, screen off after 8 min.
    var dimAfterMs:  Long = 3L * 60 * 1000
    var darkAfterMs: Long = 8L * 60 * 1000

    // ── Mode ─────────────────────────────────────────────────────────────────────
    enum class Mode { DISABLED, KEEP_ALIVE, DIM_SCREEN, DIM_AND_BLANK }

    var mode: Mode = Mode.DIM_AND_BLANK
        private set

    /**
     * Change the operating mode.  If the SDR source is currently running the
     * timer is restarted immediately so the new behaviour takes effect at once.
     */
    fun updateMode(newMode: Mode) {
        Log.i(TAG, "Mode changed: $mode → $newMode")
        mode = newMode
        if (startRequested && !suspended) start()
    }

    // ── Phase ─────────────────────────────────────────────────────────────────────
    enum class Phase { IDLE, ACTIVE, DIM, DARK }

    var currentPhase: Phase = Phase.IDLE
        private set

    // True while the SDR source is running (even if mode == DISABLED and phase stays IDLE).
    private var startRequested = false
    // True while the app window is gone (backgrounded). startRequested may still be true.
    private var suspended = false

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
        activity.setSleepScreenDimmed(true)   // explicit — don't rely on DIM having set it
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
     * Start (or restart) the keep-alive logic from the Active phase.
     * Always restores full brightness and clears any dimmed/paused state first,
     * so it is safe to call when switching modes mid-session.
     */
    fun start() {
        Log.i(TAG, "Starting keep-alive (mode=$mode)")
        startRequested = true
        cancelAllCallbacks()

        // Unconditionally restore state so switching mode from DARK/DIM is clean.
        activity.applySleepBrightness(-1f)
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)
        activity.restoreFrameRate()

        if (mode == Mode.DISABLED) {
            currentPhase = Phase.IDLE
            activity.clearKeepScreenOn()
            return
        }

        currentPhase = Phase.ACTIVE
        activity.applyKeepScreenOn()

        when (mode) {
            Mode.KEEP_ALIVE -> { /* screen stays on at full brightness indefinitely */ }
            Mode.DIM_SCREEN -> {
                handler.postDelayed(enterDimRunnable, dimAfterMs)
            }
            Mode.DIM_AND_BLANK -> {
                handler.postDelayed(enterDimRunnable,  dimAfterMs)
                handler.postDelayed(enterDarkRunnable, darkAfterMs)
                handler.postDelayed(enterEndRunnable,  DARK_DURATION_MS)
            }
            else -> {}
        }
    }

    /**
     * Called when the app window goes away (Home button, task switch).
     * Releases the wake lock and cancels callbacks immediately — Android will
     * suspend the process anyway so holding the wake lock is pointless.
     * startRequested is preserved so resume() can restart the timer when the
     * window returns.
     */
    fun suspend() {
        if (!startRequested) return
        Log.i(TAG, "Suspending keep-alive (window gone)")
        suspended = true
        cancelAllCallbacks()
        currentPhase = Phase.IDLE
        activity.applySleepBrightness(-1f)
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)
        activity.restoreFrameRate()
        activity.clearKeepScreenOn()
    }

    /**
     * Called when the app window returns (app brought back to foreground).
     * Restarts the timer from the Active phase if the SDR source is running.
     */
    fun resume() {
        if (!startRequested) return
        Log.i(TAG, "Resuming keep-alive (window back)")
        suspended = false
        start()
    }

    /**
     * Stop the keep-alive logic and restore everything to normal.
     */
    fun stop() {
        Log.i(TAG, "Stopping keep-alive")
        startRequested = false
        suspended = false
        cancelAllCallbacks()
        currentPhase = Phase.IDLE
        activity.applySleepBrightness(-1f)
        activity.clearKeepScreenOn()
        activity.setSleepScreenDimmed(false)
        activity.setSleepRenderPaused(false)
        activity.restoreFrameRate()
    }

    /**
     * Called on touch during any running phase.
     *
     * - ACTIVE  : restart the dim/dark countdown without touching brightness or the wake lock.
     * - DIM/DARK: full wake — restore brightness, cancel pending callbacks, restart from Active.
     */
    fun resetToActive() {
        when (currentPhase) {
            Phase.DIM, Phase.DARK -> {
                Log.i(TAG, "Resetting from $currentPhase phase to ACTIVE")
                start()
            }
            Phase.ACTIVE -> {
                // Just reschedule the pending runnables — brightness and wake lock are already correct.
                cancelAllCallbacks()
                when (mode) {
                    Mode.DIM_SCREEN -> {
                        handler.postDelayed(enterDimRunnable, dimAfterMs)
                    }
                    Mode.DIM_AND_BLANK -> {
                        handler.postDelayed(enterDimRunnable,  dimAfterMs)
                        handler.postDelayed(enterDarkRunnable, darkAfterMs)
                        handler.postDelayed(enterEndRunnable,  darkAfterMs + DARK_DURATION_MS)
                    }
                    else -> {}
                }
            }
            else -> {}
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
