#pragma once

#include <Arduino.h>
#include <nrf.h>

// Sleep / wake support for:
//
//   Seeed Studio XIAO nRF52840 Sense
//   + Seeed Studio Round Display for XIAO
//
// Round Display:
//   D6 = LCD backlight
//   D7 = CHSC6X touch interrupt, active LOW
//
// XIAO nRF52840 Sense:
//   D7 = P1.12
//
// Deep sleep:
//   nRF52840 SYSTEMOFF
//
// Wake:
//   CHSC6X drives D7/P1.12 LOW when the display is touched.
//   P1.12 SENSE_LOW wakes the nRF52840 from SYSTEMOFF.
//
// IMPORTANT:
// The nRF52840 cannot count the 6-second hold while actually in SYSTEMOFF.
// The first touch wakes the CPU, but the LCD stays dark. The firmware then
// validates a continuous 6-second touch. An early release immediately sends
// the board back to SYSTEMOFF.

namespace NRFSleep {

constexpr uint32_t WAKE_HOLD_MS       = 6000;
constexpr uint32_t RELEASE_STABLE_MS  = 200;

// Used to distinguish "we intentionally went to sleep"
// from a normal power-on/reset.
//
// GPREGRET2 survives SYSTEMOFF but is reset by a real power cycle.
constexpr uint8_t SLEEP_MARKER = 0x5A;


// -----------------------------------------------------------------------------
// Basic screen/touch helpers
// -----------------------------------------------------------------------------

inline bool touchPressed()
{
    // CHSC6X interrupt is active LOW.
    return digitalRead(D7) == LOW;
}


inline void backlightOff()
{
    pinMode(D6, OUTPUT);
    digitalWrite(D6, LOW);
}


inline void backlightOn()
{
    pinMode(D6, OUTPUT);
    digitalWrite(D6, HIGH);
}


// -----------------------------------------------------------------------------
// nRF52840 GPIO wake configuration
// -----------------------------------------------------------------------------
//
// XIAO D7 = physical nRF52840 P1.12.
//
// We configure P1.12 directly because the GPIO SENSE mechanism is what
// wakes the nRF52840 from SYSTEMOFF.
//

inline void configureTouchPinNoSense()
{
    pinMode(D7, INPUT_PULLUP);

    NRF_P1->PIN_CNF[12] =
        (GPIO_PIN_CNF_DIR_Input       << GPIO_PIN_CNF_DIR_Pos)   |
        (GPIO_PIN_CNF_INPUT_Connect   << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup     << GPIO_PIN_CNF_PULL_Pos)  |
        (GPIO_PIN_CNF_DRIVE_S0S1      << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled  << GPIO_PIN_CNF_SENSE_Pos);
}


inline void configureTouchWake()
{
    pinMode(D7, INPUT_PULLUP);

    NRF_P1->PIN_CNF[12] =
        (GPIO_PIN_CNF_DIR_Input      << GPIO_PIN_CNF_DIR_Pos)   |
        (GPIO_PIN_CNF_INPUT_Connect  << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup    << GPIO_PIN_CNF_PULL_Pos)  |
        (GPIO_PIN_CNF_DRIVE_S0S1     << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_SENSE_Low      << GPIO_PIN_CNF_SENSE_Pos);
}


// -----------------------------------------------------------------------------
// Wait for the user's finger to be genuinely off the screen
// -----------------------------------------------------------------------------
//
// This prevents the press of the SLEEP button from immediately becoming
// the wake press.
//

inline bool waitForTouchRelease(uint32_t timeoutMs = 5000)
{
    uint32_t start = millis();
    uint32_t releasedSince = 0;

    while ((millis() - start) < timeoutMs)
    {
        if (!touchPressed())
        {
            if (releasedSince == 0)
                releasedSince = millis();

            if ((millis() - releasedSince) >= RELEASE_STABLE_MS)
                return true;
        }
        else
        {
            releasedSince = 0;
        }

        delay(10);
    }

    return false;
}


// -----------------------------------------------------------------------------
// Enter true nRF52840 SYSTEMOFF
// -----------------------------------------------------------------------------

inline void systemOffNow()
{
    backlightOff();

    // The line should normally already be HIGH because the sleep-button
    // press was released before getting here.
    configureTouchWake();

    // Mark this as one of our intentional sleep cycles.
    NRF_POWER->GPREGRET2 = SLEEP_MARKER;

    // RESETREAS is write-1-to-clear.
    // Clear any previous reset reasons before entering SYSTEMOFF.
    uint32_t resetReasons = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = resetReasons;

    __DSB();

    // Deepest nRF52840 power state.
    NRF_POWER->SYSTEMOFF = 1;

    __DSB();

    // SYSTEMOFF should never return.
    while (true)
    {
        __WFE();
    }
}


// -----------------------------------------------------------------------------
// Determine whether setup() is running because our sleep mode woke
// -----------------------------------------------------------------------------

inline bool wokeFromOurSleep()
{
    const bool marked =
        ((NRF_POWER->GPREGRET2 & 0xFFu) == SLEEP_MARKER);

    // Also inspect the Nordic reset reason as a fallback.
    const uint32_t resetReasons = NRF_POWER->RESETREAS;

#ifdef POWER_RESETREAS_OFF_Msk
    const bool systemOffWake =
        (resetReasons & POWER_RESETREAS_OFF_Msk) != 0;
#else
    const bool systemOffWake = false;
#endif

    // Clear the retained application marker immediately.
    NRF_POWER->GPREGRET2 = 0;

    // Clear reset reasons after inspecting them.
    NRF_POWER->RESETREAS = resetReasons;

    // Normally both are true. The retained marker also makes this robust
    // against startup/bootloader code that may inspect RESETREAS.
    return marked || systemOffWake;
}


// -----------------------------------------------------------------------------
// Six-second wake gate
// -----------------------------------------------------------------------------
//
// Call this AS THE FIRST THING in setup(), before initializing the display.
//
// Normal cold boot:
//   returns immediately.
//
// Wake from SYSTEMOFF:
//   - LCD/backlight stay dark.
//   - Finger must remain continuously on screen for 6 seconds.
//   - Early release -> immediately return to SYSTEMOFF.
//   - Successful 6-second hold -> continue normal setup().
//

inline void gateWakeHoldIfNeeded()
{
    // Make absolutely sure the display does not flash during the wake check.
    backlightOff();

    configureTouchPinNoSense();

    if (!wokeFromOurSleep())
    {
        // Normal power-on or reset.
        return;
    }

    // If the board has already reached setup() after a very short tap,
    // the touch may already be released.
    if (!touchPressed())
    {
        waitForTouchRelease(500);
        systemOffNow();
    }

    const uint32_t heldSince = millis();

    while (touchPressed())
    {
        if ((millis() - heldSince) >= WAKE_HOLD_MS)
        {
            // Successful deliberate wake.
            //
            // Disable the System-OFF sense configuration before normal use.
            configureTouchPinNoSense();

            // Require release before LVGL's touch driver starts so this wake
            // gesture cannot accidentally press something in the game.
            while (touchPressed())
            {
                delay(10);
            }

            delay(RELEASE_STABLE_MS);
            return;
        }

        delay(10);
    }

    // Finger came off before six seconds.
    //
    // Debounce the release, then disappear straight back into SYSTEMOFF.
    waitForTouchRelease();

    systemOffNow();
}

} // namespace NRFSleep