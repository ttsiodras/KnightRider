/**
 * Nostalgia-driven Knight Rider K.I.T.T. 8-LED scanner bar.
 * Original by Petr Stehlik (petr@pstehlik.cz) in December 2015.
 *
 * Cleaned up, simplified and commented by Thanassis Tsiodras in 2019.
 */

#include <TimerOne.h>

#define PWM_PINS    8
#define START_PIN   2

// The 'inputs' of the SW PWM - i.e. what our logic (in moveKITT, below)
// sets our LEDs to.
bool leds[PWM_PINS];

// The intermediate outputs - what out logic (in fadeEffect, below)
// computes - from the requested 'input' LED values ('leds') - as the time
// goes on (fading them...)
//
// Eventually, the value for each LED gets to 0 - and the LED switches off.
byte pwm_regs[PWM_PINS];

void setup()
{
    for(byte i = START_PIN; i < START_PIN+PWM_PINS; i++) {
        pinMode(i, OUTPUT);
        digitalWrite(i, LOW);
    }

    // We use 64 for TimerOne param - so 1MHz / 64 = 15625Hz ISR frequency.
    // We then split these 15625 activations into 61 blocks of 256 ticks each,
    // and use them to perform 8bit PWM at 61 Hz refresh rate.
    //
    //      ,----- switch LED off here
    //      ,          ,----- switch LED on here
    //      |          |       ...etc
    //      V          V
    //   ,__           ,__           ,__           ,__
    //   |  |          |  |          |  |          |  |
    //   |  |          |  |          |  |          |  |
    //  _'  `----------'  `----------'  `----------'  `-----
    //                 <--- block -->
    //   <--- block --->   (of 256 ticks)
    //
    // Simply put, 61 times per second, we use each of the 256 ticks,
    // to set on or off the LEDs:
    //
    // - if a LED is on for all 256 ticks, it is lit at full intensity
    // - if a LED is off for all 256 ticks, it is completely dark
    // - values in between => brightness control

    Timer1.initialize(64);
    Timer1.attachInterrupt(myIrq);
}

void loop()
{
    // Do whatever you want - all of this runs in the background!
    delay(1000);
}

void myIrq(void)
{
    // This function fires at 15625Hz.
    static byte counter = 0;
    if (counter % 64 == 0) {
        // So this fires at 244Hz. Trim this to whatever you want
        // to control the speed of your KITT :-)
        moveKITT();
    }
    if (!counter++) {
        // 15625 / 256 = 61 times per second, fade up/down the LEDs
        fadeEffect();
    }
    // This is the main PWM logic - runs at the full speed of 15625Hz.
    softPWM();
}

void moveKITT(void)
{
    static byte shadow;
    static bool dirRight;
    static byte kittIndex;

    if (!shadow)
        shadow = 40; // This controls how fast K.I.T.T. will look around :-)
                     // Tweak it to your heart's content - or 'glue' it up
                     // to a potentiometer for run-time fun :-)
    if (--shadow == 0) {
        // Go left/right/left/right/...
        leds[kittIndex] = LOW;
        if (dirRight && ++kittIndex >= PWM_PINS) { kittIndex = PWM_PINS-2; dirRight = false; }
        else if (!dirRight && --kittIndex >= PWM_PINS) { kittIndex = 1; dirRight = true; }
        leds[kittIndex] = HIGH;
        // The updated 'leds' values drive the SW PWM (see fadeEffect below)
    }
}

void softPWM(void)
{
    // This fires at 15625Hz.
    // 61 times per second, we use each of the 256 ticks,
    // to set on or off the LEDs:
    //
    // - if a LED is on for all 256 ticks, it is lit at full intensity
    // - if a LED is off for all 256 ticks, it is completely dark
    // - values in between => brightness control
    static byte counter = 0;
    static byte shadows[PWM_PINS];

    if (!counter++) {
        // At the beginning of each of the 61 blocks (of 256 ticks),
        // reset the 'shadows' counters to their currently set 
        // PWM value (0..255).
        for(byte i = 0; i < PWM_PINS; i++) {
            shadows[i] = pwm_regs[i];
        }
        counter++;
    }

    // Then at each tick, decrement the 'shadows' counter of each LED.
    byte state = 0;
    for(byte i = 0; i < PWM_PINS; i++) {
        bool b = false;
        if (shadows[i]) {
            shadows[i]--;
            b = true;
        }
        // if the counter of this LED has reached zero, then
        // its state must now be switched to off.
        //
        //      ,----- counter reached zero at this point
        //      |
        //      V
        //   ,__           ,__           ,__           ,__
        //   |  |          |  |          |  |          |  |
        //   |  |          |  |          |  |          |  |
        //  _'  `----------'  `----------'  `----------'  `-----
        //                 <-- next block -->
        //   <--- block --->   (of 256 ticks)
        //
        state = (state << 1) | b;
    }

    // Use direct PORT register access instead of digitalWrite
    // (which would be abysmally slower)
    PORTD = (PORTD & 0x03) | (state << 2);
    PORTB = (PORTB & ~0x03) | (state >> 6);
}

void fadeEffect(void)
{
    for(byte i = 0; i < PWM_PINS; i++) {
        if (leds[i] && pwm_regs[i] != 255) {
            // if the main logic has requested this LED to be on,
            // and the SW PWM hasn't reached 255 yet,
            // quickly light it up (in 4 steps).
            unsigned x = pwm_regs[i] + 64;  
            if (x > 255) x = 255;
            pwm_regs[i] = x;
        }
        else if (!leds[i] && pwm_regs[i]) {
            // if the main logic has requested this LED to be off,
            // very slowly fade it out.
            pwm_regs[i] = pwm_regs[i] * 15 / 16; 
        }
    }   
}