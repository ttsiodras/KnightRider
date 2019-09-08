/**
 * Nostalgia-driven Knight Rider K.I.T.T. 8-LED scanner bar.
 * Original by Petr Stehlik (petr@pstehlik.cz) in December 2015.
 *
 * Cleaned up, simplified and commented by Thanassis Tsiodras in 2019.
 */

#include <TimerOne.h>

#define   PWM_PINS    8

#define   DATA_PIN    2
#define   CLOCK_PIN   1
#define   LATCH_PIN   0

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
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);

    // Switch off all LEDs.
    digitalWrite(LATCH_PIN, HIGH);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0);  

    // We use 64 for TimerOne param - so 1MHz / 64 = 15625Hz ISR frequency.
    // We then split these 15625 activations into 122 blocks of 128 ticks each,
    // and use them to perform 8bit PWM at 122 Hz refresh rate.
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
    // Simply put, 122 times per second, we use each of the 128 ticks,
    // to set on or off the LEDs:
    //
    // - if a LED is on for all 128 ticks, it is lit at full intensity
    // - if a LED is off for all 128 ticks, it is completely dark
    // - values in between => brightness control
    //
    // But that's not the whole story - read the comments in softPWM
    // to see why this wasn't good enough for the ATtiny85.
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
    counter++;
    if ((counter & 31) == 0) {
        // So this fires at 488Hz. Trim this to whatever you want
        // to control the speed of your KITT :-)
        moveKITT();
    }
    if ((counter & 127) == 0) {
        // 15625 / 128 = 122 times per second, fade up/down the LEDs
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
        shadow = 8; // This controls how fast K.I.T.T. will look around :-)
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
    // 122 times per second, we use each of the 128 ticks,
    // to set on or off the LEDs:
    //
    // - if a LED is on for all 256 ticks, it is lit at full intensity
    // - if a LED is off for all 256 ticks, it is completely dark
    // - values in between => brightness control
    static byte counter = 0;
    static unsigned shadows[PWM_PINS];

    counter++;
    for(byte i = 0; i < PWM_PINS; i++) {
        shadows[i] += pwm_regs[i];
    }

    // In the old implementation, at each tick, we decremented
    // the 'shadows' counter of each LED, and when it
    // reached zero, then its state would be switched to off.
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
    // The problem with this, is that the ATtiny85 has to use
    // a shift register (74HC595) to control 8 leds - and this
    // takes too much time... So much, that you can see the PWM effect,
    // i.e. you can see the LEDs flicker!
    //
    // So instead, I "spread out" the on/off times, by using
    // something like a Bresenham algorithm; I increment 'shadows'
    // by the PWM setting computed in fadeEffect; and the moment the
    // computation exceeds 128, I light the LED up (and subtract 128)
    // 
    // The result is that on average, the LED stays on the expected
    // percentage of time; but the on/offs are done all the time,
    // i.e. they are "Spread out".
    //
    // Flicker begone!
    byte state = 0;
    for(byte i = 0; i < PWM_PINS; i++) {
        bool b = false;
        if (shadows[i] > 127) {
            shadows[i] -= 128;
            b = true;
        }
        state = (state << 1) | b;
    }
    // the LEDs must not change while we're sending in bits
    digitalWrite(LATCH_PIN, LOW);
    // shift out the bits:
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, state);  
    //take the latch pin high so the LEDs will light up
    digitalWrite(LATCH_PIN, HIGH);
}

void fadeEffect(void)
{
    for(byte i = 0; i < PWM_PINS; i++) {
        if (leds[i] && pwm_regs[i] < 127) {
            // if the main logic has requested this LED to be on,
            // and the SW PWM hasn't reached 127 yet,
            // quickly light it up (in 4 steps).
            unsigned x = pwm_regs[i] + 32;  
            if (x > 127) x = 127;
            pwm_regs[i] = x;
        }
        else if (!leds[i] && pwm_regs[i]) {
            // if the main logic has requested this LED to be off,
            // very slowly fade it out.
            pwm_regs[i] = pwm_regs[i] * 13 / 16; 
        }
    }   
}
