//===========================================================================//
//                                                                           //
//  Desc:    Arduino Code to implement a fencing scoring apparatus           //
//  Dev:     Wnew                                                            //
//  Date:    Nov  2012                                                       //
//  Updated: Sept 2015                                                       //
//  Notes:   1. Basis of algorithm from digitalwestie on github. Thanks Mate //
//           2. Used uint8_t instead of int where possible to optimise       //
//           3. Set ADC prescaler to 16 faster ADC reads                     //
//                                                                           //
//  To do:   1. Could use shift reg on lights and mode LEDs to save pins     //
//           2. Implement short circuit LEDs (already provision for it)      //
//           3. Set up debug levels correctly                                //
//                                                                           //
// Alisdair: Code and ideas adapted from:                                              //
// https://github.com/wnew/fencing_scoring_box/                              //
// AND : https://www.instructables.com/Arduino-Fencing-Scoring-Apparatus/    //
//                                                                           //
//===========================================================================//
//
//  Alisdair - updates June 2020 to write to ESP8266 for remote display
//  Alisdair - write codes for hits to serial for wireless display
//  Alisdair - August 2020 - Sabre uppdated to 315 to hopefully sort out problems in outdoor damp conditions
//  Alisdair - September 2020, tidying up weapon selection at start
//  Alisdair - September 2020, adding addressable LED strips
//============
// #defines
//============
#define DEBUG 0
#define BUZZERTIME  1000  // length of time the buzzer is kept on after a hit (ms)
#define LIGHTTIME   3000  // length of time the lights are kept on after a hit (ms)
#define BAUDRATE   115200  // baudrate of the serial debug interface

// RGB LED STRIP or PANEL
#include <FastLED.h>
#define COLOR_ORDER GRB
#define NUM_LEDS_PER_STRIP 10
struct CRGB RedFencerLEDs[NUM_LEDS_PER_STRIP];
struct CRGB GreenFencerLEDs[NUM_LEDS_PER_STRIP];

//============
// Pin Setup
//============
const uint8_t shortLEDA  =  8;    // Short Circuit A Light
const uint8_t onTargetA  =  9;    // On Target A Light
const uint8_t offTargetA = 10;    // Off Target A Light
const uint8_t offTargetB = 11;    // Off Target B Light
const uint8_t onTargetB  = 12;    // On Target B Light
const uint8_t shortLEDB  = 13;    // Short Circuit A Light

const uint8_t groundPinA = A0;    // Ground A pin - Analog
const uint8_t weaponPinA = A1;    // Weapon A pin - Analog
const uint8_t lamePinA   = A2;    // Lame   A pin - Analog (Epee return path)
const uint8_t lamePinB   = A3;    // Lame   B pin - Analog (Epee return path)
const uint8_t weaponPinB = A4;    // Weapon B pin - Analog
const uint8_t groundPinB = A5;    // Ground B pin - Analog

const uint8_t modePin    =  2;        // Mode change button interrupt pin 0 (digital pin 2)
const uint8_t buzzerPin  =  3;        // buzzer pin
const uint8_t modeLeds[] = {4, 5, 6}; // LED pins to indicate weapon mode selected {f e s}


const uint8_t HITLEFT_SB  = 12; //  left touch flag for scoreboard module
const uint8_t HITRIGHT_SB = 11; // right touch flag for scoreboard module
const uint8_t OTLEFT_SB   = 10; // left off target flag for scoreboard module
const uint8_t OTRIGHT_SB  = 9; // right off target flag for scoreboard module
 
//=========================
// values of analog reads
//=========================
int weaponA = 0;
int weaponB = 0;
int lameA   = 0;
int lameB   = 0;
int groundA = 0;
int groundB = 0;

//=======================
// depress and timeouts
//=======================
long depressAtime = 0;
long depressBtime = 0;
bool lockedOut    = false;

//==========================
// Lockout & Depress Times
//==========================
// the lockout time between hits for foil is 300ms +/-25ms
// the minimum amount of time the tip needs to be depressed for foil 14ms +/-1ms
// the lockout time between hits for epee is 45ms +/-5ms (40ms -> 50ms)
// the minimum amount of time the tip needs to be depressed for epee 2ms
// the lockout time between hits for sabre is 170ms +/-10ms
// the minimum amount of time the tip needs to be depressed (in contact) for sabre 0.1ms -> 1ms
// These values are stored as micro seconds for more accuracy
//                         foil   epee   sabre
const long lockout [] = {300000,  40000, 170000};  // the lockout time between hits
const long depress [] = { 14000,   2000,   1000};  // the minimum amount of time the tip needs to be depressed

//=================
// mode constants
//=================
const uint8_t FOIL_MODE  = 0;
const uint8_t EPEE_MODE  = 1;
const uint8_t SABRE_MODE = 2;

uint8_t currentMode = EPEE_MODE;  // otherwise the buzzer is irritating until two weapons are connected.

bool modeJustChangedFlag = false;

//=========
// states
//=========
boolean depressedA  = false;
boolean depressedB  = false;
boolean hitOnTargA  = false;
boolean hitOffTargA = false;
boolean hitOnTargB  = false;
boolean hitOffTargB = false;

#ifdef TEST_ADC_SPEED
long now;
long loopCount = 0;
bool done = false;
#endif

//================
// Configuration
//================
void setup() {

  Serial.begin(BAUDRATE);
  // set the internal pullup resistor on modePin
  pinMode(modePin, INPUT_PULLUP);

  // add the interrupt to the mode pin (interrupt is pin 0)
  attachInterrupt(modePin - 2, changeMode, FALLING);
  pinMode(modeLeds[0], OUTPUT);
  pinMode(modeLeds[1], OUTPUT);
  pinMode(modeLeds[2], OUTPUT);

  // set the light pins to outputs
  pinMode(offTargetA, OUTPUT);
  pinMode(offTargetB, OUTPUT);
  pinMode(onTargetA,  OUTPUT);
  pinMode(onTargetB,  OUTPUT);
  pinMode(shortLEDA,  OUTPUT);
  pinMode(shortLEDB,  OUTPUT);
  pinMode(buzzerPin,  OUTPUT);


  // RGB LED STRIP

  FastLED.addLeds<NEOPIXEL, 13>(RedFencerLEDs, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<NEOPIXEL, 8>(GreenFencerLEDs, NUM_LEDS_PER_STRIP);

  testLights();

  digitalWrite(modeLeds[currentMode], HIGH);

  Serial.println("# WLFC 3 Weapon Scoring Box");
  Serial.println("# =========================");
  Serial.println();
  Serial.println("-@                         ");
  Serial.println(" J@p  ,                    ");
  Serial.println("  )@@@N                    ");
  Serial.println("*MNM%@      ,,,            ");
  Serial.println("     ]@NM'*~```~^`'*Nw,    ");
  Serial.println("   @'                  '@  ");
  Serial.println("   C       ,,,gg@@ ,@`  ]  ");
  Serial.println("   C     $@@@@@@@@@F    ]h ");
  Serial.println("   [  ,g@@@@@@@@@@@p    ]` ");
  Serial.println("   $  {$@@@@@@P?0@@@p   $  ");
  Serial.println("   ]h ;#@@@@@@    5M~  ,C  ");
  Serial.println("    $ 'N|%@@@@@        @   ");
  Serial.println("     %   ?Mw$N@@      @    ");
  Serial.println("      ]p     'k'    ,P     ");
  Serial.println("       `%p        ;@*      ");
  Serial.println("          ?W,  ,mP  k      ");
  Serial.println("             ?`      Y     ");
  Serial.println("                      '   ");

  Serial.println();
  Serial.println("version: 9 March 2021");
  Serial.println();

  Serial.print  ("# Mode : ");
  Serial.println(currentMode);

  resetValues();

  // alisdair - choose mode before starting

  unsigned long now;
  unsigned long startloop;
  uint8_t startMode = currentMode;
  digitalWrite(modeLeds[0],  LOW);
  digitalWrite(modeLeds[1],  LOW);
  digitalWrite(modeLeds[2],  LOW);
  // turn on current mode light
  digitalWrite(modeLeds[currentMode],  HIGH);
  startloop = millis();
  while ( millis() - startloop <= 5000) 
  {

    // search for changes in the blade mode
    if (modeJustChangedFlag) 
    {
      if (currentMode == 2) currentMode = 0;
      else currentMode++;

      setModeLeds();
      RGB_blue();
      startloop = millis();

      Serial.print("# Mode changed to: ");
      switch (currentMode) 
      {
        case 0:
          Serial.println("Foil");
          break;
        case 1:
          Serial.println("Epee");
          break;
        case 2:
          Serial.println("Sabre");
          break;
      }
      //     Serial.println(currentMode);
      modeJustChangedFlag = false;
      unsigned long currentMillis = millis();
      while ((millis() - currentMillis) < 1000) { }  // 1s pause
    }

    if (currentMode != startMode) 
    {

      // turn off mode lights
      digitalWrite(modeLeds[0],  LOW);
      digitalWrite(modeLeds[1],  LOW);
      digitalWrite(modeLeds[2],  LOW);
      RGB_blank();

      // turn on current mode light
      digitalWrite(modeLeds[currentMode],  HIGH);
      buzz();
      //    digitalWrite(buzzerPin,  HIGH);
      //    delay(100);
      //    digitalWrite(buzzerPin,  LOW);
    }
    startMode = currentMode;
    //   delay(250);
  }
}


//=============
// ADC config
//=============
//void adcOpt() {
//
// the ADC only needs a couple of bits, the atmega is an 8 bit micro
// so sampling only 8 bits makes the values easy/quicker to process
// unfortunately this method only works on the Due.
//analogReadResolution(8);

// Data Input Disable Register
// disconnects the digital inputs from which ever ADC channels you are using
// an analog input will be float and cause the digital input to constantly
// toggle high and low, this creates noise near the ADC, and uses extra
// power Secondly, the digital input and associated DIDR switch have a
// capacitance associated with them which will slow down your input signal
// if you are sampling a highly resistive load
//  DIDR0 = 0x7F;

// set the prescaler for the ADCs to 16 this allowes the fastest sampling
//  bitClear(ADCSRA, ADPS0);
//  bitClear(ADCSRA, ADPS1);
//  bitSet  (ADCSRA, ADPS2);
//}


//============
// Main Loop
//============
void loop() 
{
  beep();
  // alisdair - moved down as testlights now flashes mode lights
  digitalWrite(modeLeds[currentMode], HIGH);

  Serial.println("# Starting.........");


  // use a while as a main loop as the loop() has too much overhead for fast analogReads
  // we get a 3-4% speed up on the loop this way
  while (1) 
  {
    checkIfModeChanged();
    // read analog pins
    weaponA = analogRead(weaponPinA);
    weaponB = analogRead(weaponPinB);

    lameA   = analogRead(lamePinA);
    lameB   = analogRead(lamePinB);

    signalHits(); // this has been updated for the following:
    // if there has been a hit, send information to the scoreboard and do a short pause
      // touch left
      // touch right
      // off target left
      // off target right

    if      (currentMode == FOIL_MODE)  foil();
    else if (currentMode == EPEE_MODE)  epee();
    else if (currentMode == SABRE_MODE) sabre();

#ifdef TEST_ADC_SPEED
    if (loopCount == 0) 
    {
      now = micros();
    }
    loopCount++;
    if ((micros() - now >= 1000000) && done == false) 
    {
      Serial.print("# ");
      Serial.print(loopCount);
      Serial.println(" readings in 1 sec");
      done = true;
    }
#endif
  }
}


//=====================
// Mode pin interrupt
//=====================
void changeMode() {
  // set a flag to keep the time in the ISR to a min
  modeJustChangedFlag = true;

}


//============================
// Sets the correct mode led
//============================
void setModeLeds() 
{
  if (currentMode == FOIL_MODE) 
  {
    digitalWrite(onTargetA, HIGH);
  } 
  else 
  {
    if (currentMode == EPEE_MODE) 
    {
      digitalWrite(onTargetB, HIGH);
    } 
    else 
    {
      if (currentMode == SABRE_MODE) 
      {
        digitalWrite(onTargetA, HIGH);
        digitalWrite(onTargetB, HIGH);
      }
    }
  }

  // Alisdair
  digitalWrite(modeLeds[0],  LOW);
  digitalWrite(modeLeds[1],  LOW);
  digitalWrite(modeLeds[2],  LOW);

  // turn on current mode light
  digitalWrite(modeLeds[currentMode],  HIGH);
  RGB_blue();

  buzz();

  delay(500);
  digitalWrite(onTargetA, LOW);
  digitalWrite(onTargetB, LOW);
  RGB_blank();
}


//========================
// Run when mode changed
//========================
void checkIfModeChanged() {
  if (modeJustChangedFlag) {
    //    if (digitalRead(modePin)) {
    if (currentMode == 2)
      currentMode = 0;
    else
      currentMode++;
    //    }
    setModeLeds();


#ifdef DEBUG
    Serial.print("# Mode changed to: ");
    switch (currentMode) {
      case 0:
        Serial.println("Foil");
        break;
      case 1:
        Serial.println("Epee");
        break;
      case 2:
        Serial.println("Sabre");
        break;
    }

    //  Serial.println(currentMode);
#endif
    modeJustChangedFlag = false;
    unsigned long currentMillis = millis();
    while ((millis() - currentMillis) < 500) { }  // short pause
  }
}


//===================
// Main foil method
//===================
void foil() {

  long now = micros();
  if (((hitOnTargA || hitOffTargA) && (depressAtime + lockout[0] < now)) ||
      ((hitOnTargB || hitOffTargB) && (depressBtime + lockout[0] < now))) {
    lockedOut = true;
  }

  // weapon A
  if (hitOnTargA == false && hitOffTargA == false) { // ignore if A has already hit
    // off target
    if (900 < weaponA && lameB < 100) {
      if (!depressedA) {
        depressAtime = micros();
        depressedA   = true;
      } else {
        if (depressAtime + depress[0] <= micros()) {
          hitOffTargA = true;
        }
      }
    } else {
      // on target
      if (400 < weaponA && weaponA < 600 && 400 < lameB && lameB < 600) {
        if (!depressedA) {
          depressAtime = micros();
          depressedA   = true;
        } else {
          if (depressAtime + depress[0] <= micros()) {
            hitOnTargA = true;
          }
        }
      } else {
        // reset these values if the depress time is short.
        depressAtime = 0;
        depressedA   = 0;
      }
    }
  }

  // weapon B
  if (hitOnTargB == false && hitOffTargB == false) { // ignore if B has already hit
    // off target
    if (900 < weaponB && lameA < 100) {
      if (!depressedB) {
        depressBtime = micros();
        depressedB   = true;
      } else {
        if (depressBtime + depress[0] <= micros()) {
          hitOffTargB = true;
        }
      }
    } else {
      // on target
      if (400 < weaponB && weaponB < 600 && 400 < lameA && lameA < 600) {
        if (!depressedB) {
          depressBtime = micros();
          depressedB   = true;
        } else {
          if (depressBtime + depress[0] <= micros()) {
            hitOnTargB = true;
          }
        }
      } else {
        // reset these values if the depress time is short.
        depressBtime = 0;
        depressedB   = 0;
      }
    }
  }
}


//===================
// Main epee method
//===================
void epee() 
{
  long now = micros();
  if ((hitOnTargA && (depressAtime + lockout[1] < now)) || (hitOnTargB && (depressBtime + lockout[1] < now))) 
  {
    lockedOut = true;
  }

  // weapon A
  //  no hit for A yet    && weapon depress    && opponent lame touched
  if (hitOnTargA == false) 
  {
    if (400 < weaponA && weaponA < 600 && 400 < lameA && lameA < 600) // are these ADCs?
    {
      if (!depressedA) 
      {
        depressAtime = micros();
        depressedA   = true;
      } 
      else 
      {
        if (depressAtime + depress[1] <= micros()) 
        {
          hitOnTargA = true;
        }
      }
    }
    else 
    {
      // reset these values if the depress time is short.
      if (depressedA == true) 
      {
        depressAtime = 0;
        depressedA   = 0;
      }
    }
  }

  // weapon B
  //  no hit for B yet    && weapon depress    && opponent lame touched
  if (hitOnTargB == false) 
  {
    if (400 < weaponB && weaponB < 600 && 400 < lameB && lameB < 600) 
    {
      if (!depressedB) 
      {
        depressBtime = micros();
        depressedB   = true;
      } 
      else {
        if (depressBtime + depress[1] <= micros()) 
        {
          hitOnTargB = true;
        }
      }
    } 
    else 
    {
      // reset these values if the depress time is short.
      if (depressedB == true) 
      {
        depressBtime = 0;
        depressedB   = 0;
      }
    }
  }
}


//===================
// Main sabre method
//===================
void sabre() 
{


  long now = micros();
  if (((hitOnTargA || hitOffTargA) && (depressAtime + lockout[2] < now)) ||
      ((hitOnTargB || hitOffTargB) && (depressBtime + lockout[2] < now))) {
    lockedOut = true;
  }

  // weapon A
  if (hitOnTargA == false && hitOffTargA == false) { // ignore if A has already hit
    // on target
    if (315 < weaponA && weaponA < 600 && 300 < lameB && lameB < 600) {
      if (!depressedA) {
        depressAtime = micros();
        depressedA   = true;
      } else {
        if (depressAtime + depress[2] <= micros()) {
          hitOnTargA = true;
        }
      }
    } else {
      // reset these values if the depress time is short.
      depressAtime = 0;
      depressedA   = 0;
    }
  }

  // weapon B
  if (hitOnTargB == false && hitOffTargB == false) { // ignore if B has already hit
    // on target
    if (315 < weaponB && weaponB < 600 && 300 < lameA && lameA < 600) {
      if (!depressedB) {
        depressBtime = micros();
        depressedB   = true;
      } else {
        if (depressBtime + depress[2] <= micros()) {
          hitOnTargB = true;
        }
      }
    } else {
      // reset these values if the depress time is short.
      depressBtime = 0;
      depressedB   = 0;
    }
  }
}


//==============
// Signal Hits
//==============
void signalHits() 
{
  // non time critical, this is run after a hit has been detected

  if (lockedOut) 
  {
    // writeDisplay();

    // this will be handled by scoreboard
    // digitalWrite(onTargetA,  hitOnTargA);
    // digitalWrite(offTargetA, hitOffTargA);
    // digitalWrite(offTargetB, hitOffTargB);
    // digitalWrite(onTargetB,  hitOnTargB);
    // digitalWrite(buzzerPin,  HIGH);

    // send information to the scoreboard

    // fencer A = left fencer, fencer B = right fencer 

    // digitalWrite(HITLEFT_SB,  hitOnTargA)  // touch for left fencer
    // digitalWrite(HITRIGHT_SB, hitOnTargB) // touch for right fencer
    // digitalWrite(OTLEFT_SB,   hitOffTargA)  // off target for right fencer
    // digitalWrite(OTRIGHT_SB,  hitOffTargB)   // off target for left fencer



#ifdef DEBUG
    /*   String serData = String("# hitOnTargA  : ") + hitOnTargA  + "\n"
                        + "# hitOffTargA : "  + hitOffTargA + "\n"
                        + "# hitOffTargB : "  + hitOffTargB + "\n"
                        + "# hitOnTargB  : "  + hitOnTargB  + "\n"
                        + "# Locked Out  : "  + lockedOut   + "\n";

       Serial.println(serData);
    */
    //      writeDisplay();

#endif
    resetValues();
  }
}


//======================
// Reset all variables
//======================
void resetValues() {
  delay(BUZZERTIME);             // wait before turning off the buzzer
  digitalWrite(buzzerPin,  LOW);
  delay(LIGHTTIME - BUZZERTIME); // wait before turning off the lights
  digitalWrite(onTargetA,  LOW);
  digitalWrite(offTargetA, LOW);
  digitalWrite(offTargetB, LOW);
  digitalWrite(onTargetB,  LOW);
  digitalWrite(shortLEDA,  LOW);
  digitalWrite(shortLEDB,  LOW);

  //alisdair - turn off mode lights
  //  digitalWrite(modeLeds[0],  LOW);
  //  digitalWrite(modeLeds[1],  LOW);
  //  digitalWrite(modeLeds[2],  LOW);

  lockedOut    = false;
  depressAtime = 0;
  depressedA   = false;
  depressBtime = 0;
  depressedB   = false;

  hitOnTargA  = false;
  hitOffTargA = false;
  hitOnTargB  = false;
  hitOffTargB = false;

  delay(100);
  RGB_blank();
}


//==============
// Test lights
//==============
void testLights() {

  RGB_blank();

  digitalWrite(onTargetB,  HIGH);
  delay(100);
  digitalWrite(onTargetB,  LOW);
  digitalWrite(offTargetB, HIGH);
  delay(100);
  digitalWrite(offTargetB, LOW);
  digitalWrite(modeLeds[1],  HIGH);
  delay(100);
  digitalWrite(modeLeds[1],  LOW);
  digitalWrite(modeLeds[0],  HIGH);
  delay(100);
  digitalWrite(modeLeds[0],  LOW);

  digitalWrite(modeLeds[2],  HIGH);
  delay(100);
  digitalWrite(modeLeds[2],  LOW);

  digitalWrite(offTargetA, HIGH);
  delay(100);
  digitalWrite(offTargetA, LOW);
  digitalWrite(onTargetA,  HIGH);
  delay(100);
  digitalWrite(onTargetA,  LOW);

  digitalWrite(offTargetA, HIGH);
  digitalWrite(offTargetB, HIGH);
  delay(500);
  digitalWrite(offTargetA, LOW);
  digitalWrite(offTargetB, LOW);


  digitalWrite(onTargetA,  HIGH);
  digitalWrite(onTargetB,  HIGH);
  delay(500);
  digitalWrite(onTargetA,  LOW);
  digitalWrite(onTargetB,  LOW);

  digitalWrite(modeLeds[0],  HIGH);
  digitalWrite(modeLeds[2],  HIGH);
  digitalWrite(modeLeds[1],  HIGH);
  delay(500);
  digitalWrite(modeLeds[0],  LOW);
  digitalWrite(modeLeds[2],  LOW);
  digitalWrite(modeLeds[1],  LOW);

  buzz();


  Serial.println("RGB LEDs");

  RGB_blank();
  fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 0, 200, 0));
  fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 200, 0, 0));
  FastLED.show();

  delay(800);
  RGB_blank();

  fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 200, 200, 200));

  fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 200, 200, 200));
  FastLED.show();

  delay(800);
  RGB_blank();

  RGB_blue();


  kludge();
  FastLED.show();

  buzz();
  Serial.println("... strip end" );

}

void buzz() 
{
  tone(buzzerPin, 500, 100);
}
void beep() 
{
  tone(buzzerPin, 1000, 500);
}

// Alisdair: the following writes a two character code to the serial interface that is used by the wireless display
//           It also light s the LED strips or panels if the yare attached.

void writeDisplay() 
{

  if ( hitOnTargA ) 
  {
    Serial.println("GH");
    fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 0, 200, 0));
  }
  if ( hitOffTargA ) 
  {
    Serial.println("GM");
    fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 200, 200, 200));
  }
  if ( hitOffTargB ) 
  {
    Serial.println("RM");
    fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 200, 200, 200));
  }
  if ( hitOnTargB ) 
  {
    Serial.println("RH");
    fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 200, 0, 0));
  }

  // LED STRIP
  kludge();
  FastLED.show();

}


// Alisdair .. the following lines are for addressable LEDs.
//             Arduino UNO is pretty good with these, but expect random LEDs being lit if you rewrite for
//             ESP8266 or ESP32 (their wireless interrupts interfere with writing to LEDs)



void RGB_blank() 
{

  Serial.println("Clear LED strips");

  fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 0, 0, 0));
  fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 0, 0, 0));
  FastLED.show();
}

void RGB_blue() 
{

  Serial.println("Blue LED strips");

  fill_solid( RedFencerLEDs,   NUM_LEDS_PER_STRIP, CRGB( 0, 0, 2));
  fill_solid( GreenFencerLEDs, NUM_LEDS_PER_STRIP, CRGB( 0, 0, 2));
  FastLED.show();
}


void kludge() 
{

//  fill_solid( RedFencerLEDs,   3, CRGB( 0, 0, 8));
//  fill_solid( GreenFencerLEDs, 3, CRGB( 0, 0, 8));

}



// Alisdair .. call status(); if you want to see values written to serial at any point in the code
// Alisdair .. comes in useful for troubleshooting

void status() {


  Serial.println("======================================");

  Serial.print(" hitOnTargA :");
  Serial.println(hitOnTargA);

  Serial.print("hitOffTargA :");
  Serial.println(hitOffTargA);

  Serial.print(" hitOnTargB :");
  Serial.println(hitOnTargB);

  Serial.print("hitOffTargB :");
  Serial.println(hitOffTargB);

  Serial.print("    weaponA :");
  Serial.println(weaponA);

  Serial.print("      lameB :");
  Serial.println(lameB);

  Serial.print("    weaponB :");
  Serial.println(weaponB);

  Serial.print("      lameA :");
  Serial.println(lameA);


  Serial.println("======================================");

  delay(1000);
}
