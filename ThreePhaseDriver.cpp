/* 
 * File:   ThreePhaseDriver.cpp
 * Author: Cameron
 * 
 * Created on December 17, 2014, 3:20 PM
 */

#include <avr/pgmspace.h>

#include "ThreePhaseDriver.h"
#include "Board.h"
#include "Debug.h"


inline static void ALowOff() {PORTB &= ~(1 << 4);}
inline static void BLowOff() {PORTF &= ~(1 << 0);}
inline static void CLowOff() {PORTB &= ~(1 << 0);}

inline static void ALowOn()  {PORTB |=  (1 << 4);}
inline static void BLowOn()  {PORTF |=  (1 << 0);}
inline static void CLowOn()  {PORTB |=  (1 << 0);}

// Reserved AVR registers for timer operation so we don't need to waste time
// with context switching and such.

/**
 * Next value to write to TCCR1A in each OC interrupt.
 * 
 * Used for disabling high side drivers when using their compare match interrupt.
 */
register u1 cacheW asm("r2");

/**
 * Cached value to write to OCR1AL at the next overflow
 */
register u1 cacheA asm("r3");

/**
 * Cached value to write to OCR1BL at the next overflow
 */
register u1 cacheB asm("r4");

/**
 * Cached value to write to OCR1CL at the next overflow
 */
register u1 cacheC asm("r5");

/**
 * Cached value to write to TIMSK1 at the next overflow.
 * 
 * Used to select which interrupt will fire next
 */
register u1 cacheI asm("r6");

inline static void stageNextPhase(ThreePhaseDriver::Phase const p) {
      if (p == ThreePhaseDriver::Phase::A) cacheW = 0b10101001 & ~(1 << COM1A1);
 else if (p == ThreePhaseDriver::Phase::B) cacheW = 0b10101001 & ~(1 << COM1B1);
 else if (p == ThreePhaseDriver::Phase::C) cacheW = 0b10101001 & ~(1 << COM1C1);
}

//u1 ThreePhaseDriver::cacheA = 0;
//u1 ThreePhaseDriver::cacheB = 0;
//u1 ThreePhaseDriver::cacheC = 0;

/**
 * The overflow vector. We need to:
 *  - clear the interrupt flag of the next interrupt fast
 *  - update our OCRnx values
 *  - enable the next interrupt
 *  - return
 * 
 * None of these instructions modify any register or SREG (besides interrupt)
 */
extern "C" void TIMER1_OVF_vect() __attribute__ ((naked,__INTR_ATTRS));

void TIMER1_OVF_vect() {
 // clear timer match interrupts
 TIFR1 = cacheI;

 OCR1AL = cacheA;
 OCR1BL = cacheB;
 OCR1CL = cacheC;
 
 // Enable next interrupt
 TIMSK1 = cacheI;
 
 // Manual interrupt return
 asm ("reti" : : );
}

/**
 * Turns off the timer interrupts and clear cacheI
 */
static inline void transitionInterruptCleanupBody() {
 // Prevent future transitions from doing the same
// cacheI = 0;
 
 // If we do the above manually instead, we can same a few clock cycles later
 asm ("clr %0" : : "r"(cacheI));
 
 // Disable timer interrupts
 // Using the freshly cleared cacheI lets us save a few clock cycles later
 TIMSK1 = cacheI;
}

/**
 * Function that should generate proper interrupt entry and exit code.
 * 
 * It however will not inline and I want this interrupt to run as fast as
 * possible, so we use the manual version instead
 */
static inline void transitionInterruptCleanup() __attribute__((signal));
static inline void transitionInterruptCleanup() {
 transitionInterruptCleanupBody();
}

/**
 * Function to manually save state like the above function should
 */
static inline void transitionInterruptCleanupManual() {
 /**
  * Since we're done with all of out timed requirements, we can do the cleanup
  * slower. Instead of using dedicated registers as before, we save our context
  * like a normal interrupt using some of the stack.
  */

 // Save registers that we use
 asm ("push r0" : : );
// asm ("push r1" : : );
 
 // Save the SREG since the body clobbers it
 asm ("in r0, 0x3f" : : );
 
 // Uncomment if the body uses r0. Otherwise, why waste the time.
// asm ("push r0" : : );
 
 /** Then we do the cleanup */
 transitionInterruptCleanupBody();
 
 // And restore the context

 // If we saved r0 above, pop it back. Otherwise, save time
// asm ("pop r0" : : );
 
 // Restore SREG
 asm ("out 0x3f, r0" : : );
 
 // Restore used registers
// asm ("pop r1" : : );
 asm ("pop r0" : : );
 
 // Manual interrupt return since this function is used in naked interrupts
 asm ("reti" : : );
}

/**
 * Turns on the next next high side outputs, as prepared in cacheW, and turns on
 * the low sides who's matching high sides have been turned off
 */
static inline void prepareNextPhase() {
 TCCR1A = cacheW;

 // If the enable bit for the output module is off, we can turn on our low side
 if (!(cacheW & (1 << COM1A1))) ALowOn();
 if (!(cacheW & (1 << COM1B1))) BLowOn();
 if (!(cacheW & (1 << COM1C1))) CLowOn();
}

/**
 * Happens at the end of Phase::A
 */
extern "C" void TIMER1_COMPA_vect() __attribute__ ((naked,__INTR_ATTRS));

/**
 * Happens at the end of Phase::B
 */
extern "C" void TIMER1_COMPB_vect() __attribute__ ((naked,__INTR_ATTRS));

/**
 * Happens at the end of Phase::C
 */
extern "C" void TIMER1_COMPC_vect() __attribute__ ((naked,__INTR_ATTRS));

/**
 * Turns off the A low side driver and turns on the next one
 */
void TIMER1_COMPA_vect() {
 ALowOff();
 prepareNextPhase();
 transitionInterruptCleanupManual();
}


/**
 * Turns off the B low side driver and turns on the next one
 */
void TIMER1_COMPB_vect() {
 BLowOff();
 prepareNextPhase();
 transitionInterruptCleanupManual();
}


/**
 * Turns off the C low side driver and turns on the next one
 */
void TIMER1_COMPC_vect() {
 CLowOff();
 prepareNextPhase();
 transitionInterruptCleanupManual();
}

static const u1 minimumPhaseSwitchMatch = 50;

void ThreePhaseDriver::init() {
 // Turn off interrupts just in case
 TIMSK1 = 0;
 // And clear them all (just the ones we know about) just in case
 TIFR1 = 0b00101111;
 
 // Clear compare match registers for now
 OCR1A = 0;
 OCR1B = 0;
 OCR1C = 0;
 
 /* 
  * Set WGM for PWM Phase Correct 8-bit
  * WGM = 0b0001
  * 
  * Eventually, set all compare output modules to:
  * Clear OC1[ABC] on compare match when up-counting.
  * Set OC1[ABC] on compare match when down-counting.
  * COM1[ABC] = 0b10
  * 
  * This makes the OCR1[ABC] registers reflect the output's "high time" per cycle
  * 
  * But for now, we leave them off and let the timed sequenced turn them on as needed
  * 
  * Select the fastest clock divider ( clkIO/1 )
  * 
  * CS1 = 0b001;
  */
 
 /**
  * TCCR1A:
  * COM1A1 COM1A0 COM1B1 COM1B0 COM1C1 COM1C0 WGM11 WGM10
  * 0b   0      0      0      0      0      0     0     1
  */
 
 TCCR1A = 0b00000001;

 /**
  * TCCR1B
  * ICNC1 ICES1 ~~~~~ WGM13 WGM12 CS12 CS11 CS10
  * 0b  0     0     0     0     0    0    0    1
  */
 TCCR1B = 0b00000001;
 
 // Turn everything off
 Board::DRV::AL.off();
 Board::DRV::BL.off();
 Board::DRV::CL.off();
 Board::DRV::AH.off();
 Board::DRV::BH.off();
 Board::DRV::CH.off();
 
 // Enable outputs
 Board::DRV::AL.output();
 Board::DRV::BL.output();
 Board::DRV::CL.output();
 Board::DRV::AH.output();
 Board::DRV::BH.output();
 Board::DRV::CH.output();
}

static const u1 limitedSinTable[ThreePhaseDriver::StepsPerPhase] PROGMEM = {
   0,  2,  4,  6,  8, 10, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
  33, 35, 37, 39, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64,
  66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96,
  98,100,101,103,105,107,109,111,113,115,117,118,120,122,124,126,
 128,129,131,133,135,136,138,140,142,143,145,147,149,150,152,154,
 155,157,159,160,162,163,165,167,168,170,171,173,174,176,177,179,
 180,182,183,185,186,188,189,190,192,193,194,196,197,198,200,201,
 202,204,205,206,207,208,210,211,212,213,214,215,217,218,219,220,
 221,222,223,224,225,226,227,228,229,230,231,231,232,233,234,235,
 236,236,237,238,239,239,240,241,241,242,243,243,244,245,245,246,
 246,247,247,248,248,249,249,250,250,250,251,251,252,252,252,253,
 253,253,253,254,254,254,254,254,254,255,255,255,255,255,255,255,
 255,255,255,255,255,255,255,255,254,254,254,254,254,254,253,253,
 253,253,252,252,252,251,251,250,250,250,249,249,248,248,247,247,
 246,246,245,245,244,243,243,242,241,241,240,239,239,238,237,236,
 236,235,234,233,232,231,231,230,229,228,227,226,225,224,223,222,
};

u1 ThreePhaseDriver::amplitude = 0;

u1 ThreePhaseDriver::getPhasePWM(const u1 step) {
// u1 const sin = MAX * SIN(2 * PI * step / StepsPerCycle);
 u1 const sin = pgm_read_byte(&limitedSinTable[step]);
 
 // TODO: This product (and subsequent truncation) does not fully cover the
 // range of the return u1. Ideally, instead of dividing by 256 (>> 8) we should
 // be dividing by 255. We can get closer, on average, to that ideal division
 // if we add 188 for instance. Here we add both values that are bing multiplied
 // as a slightly better approximation at the extremes.
 u2 const prod = sin * amplitude + sin + amplitude;
 
 // Ideal:
// return prod / 255;
 
 // Close enough:
 return prod >> 8;
}

void ThreePhaseDriver::advance() {
 static u2 step = 0;
 advanceTo(step);
 Debug::reportByte(step >> 2);
 if (++step == 0x300) step = 0;
}

ThreePhaseDriver::Phase ThreePhaseDriver::currentPhase = Phase::INIT;

void ThreePhaseDriver::advanceToFullSine(const Phase phase, const u1 step) {
 // If this timer's interrupts are on, don't mess with a running change
 if (TIMSK1) return;
 
 u1 const ONE = getPhasePWM(    step);
 u1 const TWO = getPhasePWM(255-step);
 u1 const max = ONE > TWO ? ONE : TWO;

 // Prevent us from using a MAX that is too low
 u1 const MAX = max > minimumPhaseSwitchMatch ? max : minimumPhaseSwitchMatch;
 
 if (phase == Phase::A) {
  cacheA = MAX;
  cacheB = TWO;
  cacheC = ONE;
  stageNextPhase(Phase::A);
 } else if (phase == Phase::B) {
  cacheA = ONE;
  cacheB = MAX;
  cacheC = TWO;
  stageNextPhase(Phase::B);
 } else if (phase == Phase::C) {
  cacheA = TWO;
  cacheB = ONE;
  cacheC = MAX;
  stageNextPhase(Phase::C);
 } else {
  // Should not get here. bad phase...
  
  cacheA = 0;
  cacheB = 0;
  cacheC = 0;
  cacheI = 0;
  return;
 }
 
 if (currentPhase != phase) {
  if (currentPhase == Phase::A) cacheI = 1 << OCF1A;
  else if (currentPhase == Phase::B) cacheI = 1 << OCF1B;
  else if (currentPhase == Phase::C) cacheI = 1 << OCF1C;
  else {
   // We must be initializing. The main purpose of cacheI (where is is used) is
   // is to disable the old low, enable the new one, and actually enable the OCR
   // outputs. We could just do nothing here and let one commutation in software
   // happen and then the outputs would be setup correctly. In the interest of
   // stating up faster, let's manually just turn on the right low side and
   // enable the outputs. Make sure this only happens when the currently locked
   // OCR values are all 0.
   
   // This does everything for us :)
   prepareNextPhase();
  }
 }

 // Clear the overflow flag (so that we don't immediately trigger an overflow))
 TIFR1 |= 1 << TOV1;
 // And enable the overflow interrupt
 TIMSK1 = 1 << TOV1;

 // Save current phase
 currentPhase = phase;
}

void ThreePhaseDriver::advanceToBackEMF(const Phase phase, const u1 step) {
 // If this timer's interrupts are on, don't mess with a running change
 if (TIMSK1) return;
 
 u1 const ONE = getPhasePWM(step | 0b10000000);
 
 u1 const MAX = ONE > minimumPhaseSwitchMatch ? ONE : minimumPhaseSwitchMatch;
 
 if (phase == currentPhase) {
  if (phase == Phase::A) {
   if (step & 0b10000000) {
    OCR1BL = 0;
    OCR1CL = ONE;
    OCR1AL = MAX;
   } else {
    OCR1CL = 0;
    OCR1BL = ONE;
    OCR1AL = MAX;
   }
  } else if (phase == Phase::B) {
   if (step & 0b10000000) {
    OCR1CL = 0;
    OCR1AL = ONE;
    OCR1BL = MAX;
   } else {
    OCR1AL = 0;
    OCR1CL = ONE;
    OCR1BL = MAX;
   }
  } else if (phase == Phase::C) {
   if (step & 0b10000000) {
    OCR1AL = 0;
    OCR1BL = ONE;
    OCR1CL = MAX;
   } else {
    OCR1BL = 0;
    OCR1AL = ONE;
    OCR1CL = MAX;
   }
  } else {
   // TODO: Error. We should not get here.
  }
  // We're done if this was a trivial duty cycle update
  return;
 }
 
 // If we are not at the same phase, we basically just need to update the OCR
 // for the current ONE and alternate which low is on. This however should
 // follow the same timing requirements as the low speed commutation to prevent
 // shoot-through. We can therefore use the same interrupts.

 if (phase == Phase::A) {
  if (step & 0b10000000) {
   cacheA = MAX;
   cacheB = 0;
   cacheC = ONE;
  } else {
   cacheA = MAX;
   cacheB = ONE;
   cacheC = 0;
  }
 } else if (phase == Phase::B) {
  if (step & 0b10000000) {
   cacheA = ONE;
   cacheB = MAX;
   cacheC = 0;
  } else {
   cacheA = 0;
   cacheB = MAX;
   cacheC = ONE;
  }
 } else if (phase == Phase::C) {
  if (step & 0b10000000) {
   cacheA = 0;
   cacheB = ONE;
   cacheC = MAX;
  } else {
   cacheA = ONE;
   cacheB = 0;
   cacheC = MAX;
  }
 } else {
  // Should not get here. bad phase...
  
  cacheA = 0;
  cacheB = 0;
  cacheC = 0;
  cacheI = 0;
  return;
 }
 
 stageNextPhase(phase);

 // Stage the correct interrupt to turn off the current phase's low driver
      if (currentPhase == Phase::A) cacheI = 1 << OCF1A;
 else if (currentPhase == Phase::B) cacheI = 1 << OCF1B;
 else if (currentPhase == Phase::C) cacheI = 1 << OCF1C;
 // we must be initializing
 else prepareNextPhase();

 // Clear the overflow flag (so that we don't immediately trigger an overflow))
 TIFR1 |= 1 << TOV1;
 // And enable the overflow interrupt
 TIMSK1 = 1 << TOV1;

 // Update where we last were
 currentPhase = phase;

}
