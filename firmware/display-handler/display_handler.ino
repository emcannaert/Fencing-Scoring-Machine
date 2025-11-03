#include "LedControl.h"
#include "IRremote.hpp"
#include "binary.h"

///////////////////////////////////////
//////// Display_handler.ino //////////
///////////////////////////////////////
/// Written by Ethan Cannaert, 2025 ///
///////////////////////////////////////

// Implements a fencing scoring display with the following features:
//   - time w/ decimal precision for t < 10s
//   - scores
//   - cards
//   - priority
//   - period tracking
//   - physical button & remote support
//   - sound feedback w/ mute option


#define DEBUG 1 

#if DEBUG
  #define DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif


/////////////
// pin map //
/////////////

constexpr uint8_t RIGHT_PRIORITY_PIN   = 34;
constexpr uint8_t LEFT_PRIORITY_PIN    = 32;

constexpr uint8_t LEFT_CARD_R_PIN  = 2;
constexpr uint8_t RIGHT_CARD_R_PIN = 3;
constexpr uint8_t LEFT_CARD_Y_PIN  = 4;
constexpr uint8_t RIGHT_CARD_Y_PIN = 5;
constexpr uint8_t BUTTON_SOUND_PIN = 6;
constexpr uint8_t PERIOD_pin = 7;
constexpr uint8_t IR_RECEIVE_PIN = 52;

constexpr uint8_t REST_TIME = 5;
constexpr uint16_t BREAK_PAUSE = 4000;

///////////
// debug //
//////////
bool debug = true;
/////////////



// Consts and LED pin mappings
unsigned long buttonCode = 0;

// DIN, CLK, CS, number of devices
LedControl lc = LedControl(12, 11, 10, 4); // 4 modules
LedControl ls = LedControl(40, 42, 44, 2); // 2 modules
LedControl rs = LedControl(46, 48, 50, 2); // 2 modules

constexpr uint16_t ONE_SECOND_MILLIS     = 1000; 
constexpr uint16_t SOUND_MILLIS          = 200; // 200 ms for beep sound to go off 
constexpr uint16_t SOUND_MILLIS_LONG     = 2000; // 200 ms for beep sound to go off 
constexpr uint16_t MATCH_LENGTH          = 20;

unsigned long previousMillis_clock = 0;
unsigned long previousMillis_sound = 0;
unsigned long previousMillis_sound_long = 0;
unsigned long previousMillis_break_pause = 0;
unsigned long previousMillis_reset_score = 0;
unsigned long previousMillis_reset_time  = 0;


// Formatted time storage

struct formattedTime 
{
  int minutes_tens;
  int minutes_ones;
  int seconds_tens;
  int seconds_ones;

   formattedTime(int t) : minutes_tens( (t/600) ), minutes_ones( (t/60)%10 ), seconds_tens( (t%60)/10  ), seconds_ones( (t%60)%10 )  {}
};

// Container to hold all variables of "state"

struct State
{
  bool paused = true;
  bool playSound              = false;      //  should the sound be playing
  bool playSoundLong          = false;      //  should the sound be playing
  bool soundIsPlaying         = false;      // is the sound currently playing?
  bool soundIsPlayingLong     = false;      // is the sound currently playing?
  uint8_t period              = 1;          // which period?
  bool isBreak                = false;      
  bool updateTime             = false;      // true when there has been a change in the time     
  bool updateTimeThreeDigits  = false;      // true when there has been a change in the time & t < 10s (when more digits are used)
  bool pausedBeforeBreak      = false;      
  uint8_t reset_time          = 0;          // for restting the time with a double click on remote
  uint8_t reset_scores        = 0;          // for restting the score with a double click on remote
  bool left_card_LED_R_on     = false;      // flags for keeping track of card LED statuses
  bool left_card_LED_Y_on     = false; 
  bool right_card_LED_R_on    = false; 
  bool right_card_LED_Y_on    = false;
  float previousTime          = 0;          // keep track of previous time to cut back on how often displays are updated (which is slow)
  int8_t priority             = -1;        
  bool updatePeriodLight      = true;      
  bool score_left_changed     = false;      // keep track of previous scores to cut back on how often displays are updated (which is slow)
  bool score_right_changed    = false;    
  uint16_t timeLeft_int;
  formattedTime previousTimeForm{0};
  float timeLeft = (float) MATCH_LENGTH;
};
State STATE;  // Save initial values
const State DEFAULT_STATE{};     // not necessary, but makes resetting simple

// container for the number of cards
struct Cards
{
  uint8_t cards_Y_left  = 0;   //  numbers of yellow cards for left fencer, 0 == none, 1 == yellow, 2+ == red
  uint8_t cards_R_left  = 0;   //  numbers of yellow cards for left fencer, 0 == none, 1 == yellow, 2+ == red
  uint8_t cards_Y_right = 0;   //  numbers of yellow cards for right fencer, 0 == none, 1 == yellow, 2+ == red
  uint8_t cards_R_right = 0;   //   numbers of yellow cards for right fencer, 0 == none, 1 == yellow, 2+ == red
};
Cards CARDS;
const Cards DEFAULT_CARDS{};

// container for the current scores
struct Score
{     
  uint8_t current_score_left     = 0;    
  uint8_t current_score_right    = 0;  
  uint16_t previous_score_left   = 0;  
  uint16_t previous_score_right  = 0; 
};
Score SCORE;
const Score DEFAULT_SCORE{};

bool period_lights_lit[]  = {false,false,false};


// these are the patterns for each number (0-9) used to light up the dot matrices 
const byte numbers[10][8] PROGMEM = 
{
  {B00111100,B01000010,B01000010,B01000010,B01000010,B01000010,B01000010,B00111100}, // 0
  {B00010000,B00110000,B00010000,B00010000,B00010000,B00010000,B00010000,B00111000}, // 1
  {B00111100,B00000010,B00000010,B00011100,B00100000,B01000000,B01000000,B01111110}, // 2
  {B00111100,B00000010,B00000010,B00011100,B00000010,B00000010,B00000010,B00111100}, // 3
  {B00000100,B00001100,B00010100,B00100100,B01111110,B00000100,B00000100,B00000100}, // 4
  {B01111110,B01000000,B01000000,B01111100,B00000010,B00000010,B01000010,B00111100}, // 5
  {B00111100,B01000010,B01000000,B01111100,B01000010,B01000010,B01000010,B00111100}, // 6
  {B01111110,B00000010,B00000100,B00001000,B00010000,B00100000,B00100000,B00100000}, // 7
  {B00111100,B01000010,B01000010,B00111100,B01000010,B01000010,B01000010,B00111100}, // 8
  {B00111100,B01000010,B01000010,B00111110,B00000010,B00000010,B01000010,B00111100}  // 9
};

void translateIR() // program the behavior of the IR codes received from remote
{          
// describing Remote IR codes 
  switch(buttonCode){
    case 0xFFA25D: DEBUG_PRINTLN("POWER"); break;       
    case 0xFFE21D: DEBUG_PRINTLN("FUNC/STOP");  STATE.priority = random(0, 2);  break;    
    case 0xFF629D: DEBUG_PRINTLN("VOL+"); STATE.period++;  break;
    case 0xFF22DD: DEBUG_PRINTLN("FAST BACK");    break;
    case 0xFF02FD: DEBUG_PRINTLN("PAUSE");  STATE.period--;  break;
    case 0xFFC23D: DEBUG_PRINTLN("FAST FORWARD");   break;
    case 0xF807FF00: 
      DEBUG_PRINTLN("DOWN"); 
      if(CARDS.cards_Y_left >= 1) // if a yellow card is already present, upgrade to red and give point to right
      {
        CARDS.cards_R_left++;
        SCORE.current_score_right++;
        STATE.score_right_changed = true;
      }
      CARDS.cards_Y_left++;
      STATE.playSound = true;
      break;
    case 0xFFA857: DEBUG_PRINTLN("VOL-");    break;
    case 0xF609FF00:
      DEBUG_PRINTLN("UP"); 
      if(CARDS.cards_Y_right >= 1) // if a yellow card is already present, upgrade to red and give point to right
      {
        CARDS.cards_R_right++;
        SCORE.current_score_left++;
        STATE.score_left_changed = true;
      }
      CARDS.cards_Y_right++;  
      STATE.playSound = true; 
      break;
    case 0xE619FF00: DEBUG_PRINTLN("EQ"); STATE.reset_time +=1; previousMillis_reset_time = millis();  break;
    case 0xF20DFF00: 
      DEBUG_PRINTLN("ST/REPT"); 
      SCORE.current_score_left++;
      STATE.score_left_changed = true;
      CARDS.cards_R_right++;   
      STATE.playSound=true;
      break;
    case 0xE916FF00:
      DEBUG_PRINTLN("0");
      SCORE.current_score_right++;
      STATE.score_right_changed = true;
      CARDS.cards_R_left++;
      STATE.playSound=true;
      break;
    case 0xFF30CF: DEBUG_PRINTLN("1");    break;
    case 0xE718FF00: DEBUG_PRINTLN("2"); STATE.reset_scores +=1; previousMillis_reset_score = millis(); break;
    case 0xFF7A85: DEBUG_PRINTLN("3");    break;
    case 0xF708FF00: DEBUG_PRINTLN("4"); SCORE.current_score_left++; STATE.score_left_changed = true;  STATE.playSound=true;   break;
    case 0xE31CFF00: DEBUG_PRINTLN("5"); STATE.paused = !STATE.paused;  STATE.playSound=true;   break;
    case 0xA55AFF00: DEBUG_PRINTLN("6"); SCORE.current_score_right++; STATE.score_right_changed = true; STATE.playSound=true;  break;
    case 0xBD42FF00: DEBUG_PRINTLN("7"); if(SCORE.current_score_left >0)SCORE.current_score_left--;  STATE.playSound=true; STATE.score_left_changed = true;  break;
    case 0xFF4AB5: DEBUG_PRINTLN("8");    break;
    case 0xB54AFF00: DEBUG_PRINTLN("9"); if(SCORE.current_score_right>0)SCORE.current_score_right--; STATE.score_right_changed = true; STATE.playSound=true;   break;
    case 0xFFFFFFFF: DEBUG_PRINTLN(" REPEAT");break;  

  default: 
    DEBUG_PRINT(" other button: 0x");
    DEBUG_PRINTLN(buttonCode,HEX);

  }// End Case
  if(SCORE.current_score_left > 99) SCORE.current_score_left = 0;
  else if(SCORE.current_score_left < 0) SCORE.current_score_left = 0;

  if(SCORE.current_score_right > 99) SCORE.current_score_right = 0;
  else if(SCORE.current_score_right < 0) SCORE.current_score_right = 0;
} //END translateIR


void setup() // commands to run once arduino is turned on
{

  // set random seed for priority
  randomSeed(analogRead(A7)); 

  Serial.begin(9600);

  // set all output pins
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  pinMode(BUTTON_SOUND_PIN, OUTPUT);

  pinMode(LEFT_CARD_R_PIN, OUTPUT);
  pinMode(RIGHT_CARD_R_PIN, OUTPUT);
  pinMode(LEFT_CARD_Y_PIN, OUTPUT);
  pinMode(RIGHT_CARD_Y_PIN, OUTPUT);

  pinMode(PERIOD_pin, OUTPUT); // pin 8
  pinMode(PERIOD_pin+1, OUTPUT); // pin 9
  pinMode(PERIOD_pin+2, OUTPUT); // pin 10

  // reset the dot matrices
  for (int iii = 0; iii < 4; iii++) 
  {
    lc.shutdown(iii, false);   // Wake up
    lc.setIntensity(iii, 8);   // Brightness (0–15)
    lc.clearDisplay(iii);      // Clear
  }
  for (int iii = 0; iii < 2; iii++) 
  {
    ls.shutdown(iii, false);   // Wake up
    ls.setIntensity(iii, 8);   // Brightness (0–15)
    ls.clearDisplay(iii);      // Clear

    rs.shutdown(iii, false);   // Wake up
    rs.setIntensity(iii, 8);   // Brightness (0–15)
    rs.clearDisplay(iii);      // Clear
  }


  // initialize the time and write it to the time display
  STATE.timeLeft_int = (int) STATE.timeLeft;
  formattedTime timeLeft_form{STATE.timeLeft_int};
  for(int row=0; row<8; row++)
  {
    if(timeLeft_form.minutes_tens == 0) lc.clearDisplay(3); 
    else
    {
      lc.setRow(3, row,  pgm_read_byte(&numbers[timeLeft_form.minutes_tens][row]));  // time left minutes tens
    }

    lc.setRow(2, row,  pgm_read_byte(&numbers[timeLeft_form.minutes_ones][row]));  // time left minutes ones
    lc.setRow(1, row,  pgm_read_byte(&numbers[timeLeft_form.seconds_tens][row]));  // time left seconds tens
    lc.setRow(0, row,  pgm_read_byte(&numbers[timeLeft_form.seconds_ones][row]));  // time left seconds ones
  }
}

void loop() 
{

  // get current time 
  unsigned long currentMillis = millis();
  
  // read in any IR signals from remote
  if (IrReceiver.decode())
  {
    buttonCode = IrReceiver.decodedIRData.decodedRawData;

    DEBUG_PRINT("Raw: 0x");
    DEBUG_PRINTLN(IrReceiver.decodedIRData.decodedRawData, HEX);
    translateIR(); 

    if((STATE.period > 3) || (STATE.period < 1)) STATE.period = 1;
    IrReceiver.resume();
  }


  // update the clock
  if ( (!STATE.paused) ) 
  {
    if(STATE.pausedBeforeBreak)
    {
      // DEBUG_PRINT("currentMillis -  previousMillis_break_pause is ");
      // DEBUG_PRINTLN(currentMillis -  previousMillis_break_pause);
      if ( currentMillis -  previousMillis_break_pause > BREAK_PAUSE) 
      {
        STATE.timeLeft = (float)REST_TIME;
        STATE.pausedBeforeBreak = false;   // pause between end of match and start of break
        previousMillis_clock = currentMillis;
      }
    }
    else if ( (STATE.timeLeft >= 11.0) && ( currentMillis - previousMillis_clock >= ONE_SECOND_MILLIS ))
    {
      previousMillis_clock = currentMillis;
      STATE.updateTime = true;
      STATE.timeLeft -= 1;
    }
    else if(  (STATE.timeLeft < 11.0) && (STATE.timeLeft >0) ) // will go as fast as this can calculate
    {
      STATE.updateTimeThreeDigits = true;
      STATE.timeLeft -=  (currentMillis - previousMillis_clock)/1000.; 
      previousMillis_clock = currentMillis;
    }
    else if((STATE.timeLeft <= 0) ) // start a break
    {
      
      STATE.timeLeft = 0;
      STATE.updateTimeThreeDigits = true;

      STATE.playSoundLong = true;

      if (!STATE.isBreak) // if t == 0 and a break hasn't started, start a break 
      {
        if(STATE.period < 3) 
        {
          previousMillis_clock = currentMillis; 
          STATE.isBreak = true;
          STATE.paused = false; 

          STATE.pausedBeforeBreak = true;
          STATE.updateTimeThreeDigits = true;
          previousMillis_break_pause = currentMillis;
        }
        else { STATE.paused= true;}
      }
      else if (STATE.isBreak) // if t == 0 and a break is ongoing, move to the next period
      {
        if(STATE.period < 3)
        {
          STATE.timeLeft = (float)MATCH_LENGTH;
          previousMillis_clock = currentMillis;
          STATE.isBreak = false;
          STATE.paused = true;
          STATE.updatePeriodLight = true;
          STATE.period++;

          if( STATE.timeLeft > 10) STATE.updateTime = true;
          else {STATE.updateTimeThreeDigits = true;  }
          
          

        }
        else {STATE.paused = true; }
      }
      else { STATE.paused = true;}
    }
    if ((STATE.timeLeft < 0)) STATE.timeLeft = 0.; // error avoidance
  }

  // DEBUG_PRINT("timeLeft is ");
  // DEBUG_PRINTLN(STATE.timeLeft);

  // DEBUG_PRINT("period is ");
  // DEBUG_PRINTLN(STATE.period);

  // DEBUG_PRINT("isBreak is ");
  // DEBUG_PRINTLN(STATE.isBreak);

  // DEBUG_PRINT("paused is ");
  // DEBUG_PRINTLN(STATE.paused);

  // DEBUG_PRINT("playSoundLong is ");
  // DEBUG_PRINTLN(STATE.playSoundLong);

  // DEBUG_PRINT("soundIsPlayingLong is ");
  // DEBUG_PRINTLN(STATE.soundIsPlayingLong);

  // DEBUG_PRINT("pausedBeforeBreak is ");
  // DEBUG_PRINTLN(STATE.pausedBeforeBreak);

  // DEBUG_PRINT("STATE.updateTimeThreeDigits is");
  // DEBUG_PRINTLN(STATE.updateTimeThreeDigits);

  // DEBUG_PRINT("STATE.updateTime  is");
  // DEBUG_PRINTLN(STATE.updateTime);


  // reset the score if the user pressed the reset score remote buttom twice within 1.5 seconds
  if((STATE.reset_scores == 1) && ( (currentMillis - previousMillis_reset_score) > 1500)) // if button was pressed, but it has been more than 1.5 seconds, reset this value
  {
    STATE.reset_scores = 0;
  }
  else if(STATE.reset_scores >= 2)
  {
    SCORE.current_score_left = 0;
    SCORE.current_score_right = 0;
    STATE.reset_scores = 0;
    STATE.playSound = true;
  }
  else if((STATE.reset_time > 0) && (STATE.isBreak))
  {
    // if it is a break and the reset time button is pressed, skip break and prepare next period
    STATE.timeLeft = 0;
    STATE.updateTime = true;
  }
  
  if((STATE.reset_time == 1) && (  (currentMillis - previousMillis_reset_time) > 1500)) // if button was pressed, but it has been more than 1.5 seconds, reset this value
  {
    STATE.reset_time = 0;
  }
  else if(STATE.reset_time >= 2)
  {
    STATE.timeLeft = (float)MATCH_LENGTH;
    STATE.paused = true;
    STATE.playSound = true;
    STATE.reset_time = 0;
    // force a reset 
    STATE.updateTime = true;
  }
  
  // DEBUG_PRINT("playSound is ");
  // DEBUG_PRINTLN(playSound);

  // DEBUG_PRINT("soundIsPlaying is ");
  // DEBUG_PRINTLN(soundIsPlaying);

  // DEBUG_PRINT("reset_scores is ");
  // DEBUG_PRINTLN(reset_scores);

  // DEBUG_PRINT("reset_time is ");
  // DEBUG_PRINTLN(reset_time);

  // DEBUG_PRINT("cards_Y_right is ");
  // DEBUG_PRINTLN(cards_Y_right);

  // DEBUG_PRINT("cards_Y_left is ");
  // DEBUG_PRINTLN(cards_Y_left);

  // DEBUG_PRINT("cards_R_right is ");
  // DEBUG_PRINTLN(cards_R_right);

  // DEBUG_PRINT("cards_R_left is ");
  // DEBUG_PRINTLN(cards_R_left);




  ///////////////////////////
  // deal with card lights //
  ///////////////////////////


  if((!STATE.right_card_LED_Y_on) && (CARDS.cards_Y_right > 0))
  {
    if( CARDS.cards_Y_right > 0 ) 
    {
      digitalWrite(RIGHT_CARD_Y_PIN, HIGH);
      STATE.right_card_LED_Y_on = true;
    }
  }
  else if(!STATE.right_card_LED_R_on && (CARDS.cards_Y_right > 1)  ) 
  {
    digitalWrite(RIGHT_CARD_R_PIN, HIGH);
    STATE.right_card_LED_R_on = true;
  }
  if((!STATE.right_card_LED_R_on) && (CARDS.cards_R_right > 0))
  {
    digitalWrite(RIGHT_CARD_R_PIN, HIGH);
    STATE.right_card_LED_R_on = true;
  }
  if((!STATE.left_card_LED_Y_on) && (CARDS.cards_Y_left > 0))
  {
    if( CARDS.cards_Y_left == 1 ) 
    {
      digitalWrite(LEFT_CARD_Y_PIN, HIGH);
      STATE.left_card_LED_Y_on = true;
    }
  }
  else if((!STATE.left_card_LED_Y_on) &&  (CARDS.cards_Y_left > 1) )
  {
    digitalWrite(LEFT_CARD_R_PIN, HIGH);
    STATE.left_card_LED_R_on = true;
  }
  if((!STATE.left_card_LED_R_on) && (CARDS.cards_R_left > 0))
  {
    digitalWrite(LEFT_CARD_R_PIN, HIGH);
    STATE.left_card_LED_R_on = true;
  }


  // DEBUG_PRINT("right_card_LED_Y_on is ");
  // DEBUG_PRINTLN(right_card_LED_Y_on);

  // DEBUG_PRINT("right_card_LED_R_on is ");
  // DEBUG_PRINTLN(right_card_LED_R_on);
  
  // DEBUG_PRINT("left_card_LED_R_on is ");
  // DEBUG_PRINTLN(left_card_LED_R_on);

  // DEBUG_PRINT("left_card_LED_R_on is ");
  // DEBUG_PRINTLN(left_card_LED_R_on);




  ////////////////////
  // check priority //
  ////////////////////

  if (STATE.priority > -1)
  {
    if(STATE.priority == 0) digitalWrite(RIGHT_PRIORITY_PIN, HIGH);
    else if(STATE.priority == 1) digitalWrite(LEFT_PRIORITY_PIN, HIGH);
    STATE.priority = -1; // reset priority
  }


  ////////////////////////////////////
  // Light up the period indicators //
  ////////////////////////////////////

  if(STATE.updatePeriodLight)
  { 
    for(int iii = 0; iii<3; iii++)
    {
      if( !period_lights_lit[iii] && (STATE.period == (iii+1))  )
      {
        digitalWrite(PERIOD_pin + iii , HIGH);
        period_lights_lit[iii] = true;
      }
    }
    STATE.updatePeriodLight = false; 
  }


  /////////////////
  // play buzzer //
  /////////////////

  // play short sound (e.g. for button presses of remote)
  if(STATE.playSound)
  {
    // if the pin is on low; set it to high, set the "previous time"
    if(!STATE.soundIsPlaying)
    {
      digitalWrite(6, HIGH);
      previousMillis_sound = currentMillis;
      STATE.soundIsPlaying = true;
    }

    if((currentMillis - previousMillis_sound) > SOUND_MILLIS   ) 
    {
      digitalWrite(6, LOW);  // set pin back to low
      STATE.soundIsPlaying = false; // sound turned off
      STATE.playSound = false; // sound has been played, wait for next button press
    }
    
  } 
  // play long song (e.g. when a touch has been scored or a period ends)
  if(STATE.playSoundLong)
  {

    //currentMillis = millis();

    // if the pin is on low; set it to high, set the "previous time"
    if(!STATE.soundIsPlayingLong)
    {
      digitalWrite(6, HIGH);
      previousMillis_sound_long = currentMillis;
      STATE.soundIsPlayingLong = true;
    }

    if((currentMillis - previousMillis_sound_long) > SOUND_MILLIS_LONG   ) 
    {
      digitalWrite(6, LOW);  // set pin back to low
      STATE.soundIsPlayingLong = false; // sound turned off
      STATE.playSoundLong = false; // sound has been played, wait for next button press
    }
  } 

  // do final updating of time
  if(STATE.updateTime)
  {
    STATE.timeLeft_int = (int) STATE.timeLeft;
    formattedTime timeLeft_form{STATE.timeLeft_int};

    for(int row=0; row<8; row++)
    {
      if (timeLeft_form.minutes_tens != STATE.previousTimeForm.minutes_tens)
      {
        if( timeLeft_form.minutes_tens == 0)  lc.clearDisplay(3); 
        else {lc.setRow(3, row,  pgm_read_byte(&numbers[timeLeft_form.minutes_tens][row]));   } // time left minutes tens 
      }
      if(timeLeft_form.minutes_ones != STATE.previousTimeForm.minutes_ones) lc.setRow(2, row,  pgm_read_byte(&numbers[timeLeft_form.minutes_ones][row]));  // time left minutes ones
      if(timeLeft_form.seconds_tens != STATE.previousTimeForm.seconds_tens) lc.setRow(1, row,  pgm_read_byte(&numbers[timeLeft_form.seconds_tens][row]));  // time left seconds tens
      if(timeLeft_form.seconds_ones != STATE.previousTimeForm.seconds_ones) lc.setRow(0, row,  pgm_read_byte(&numbers[timeLeft_form.seconds_ones][row]));  // time left seconds ones
    }
    STATE.updateTime = false;
    STATE.previousTime = STATE.timeLeft; 
    STATE.previousTimeForm = timeLeft_form;
  }
  else if (STATE.updateTimeThreeDigits)
  {

    // get formatted 3-digit time to draw
    unsigned int intPart = (int)STATE.timeLeft;        
    unsigned int fracPart = (int)((STATE.timeLeft - intPart) * 100);

    uint8_t d1 = intPart / 10;    
    uint8_t d2 = intPart % 10;    
    uint8_t d3 = fracPart / 10;   
    uint8_t d4 = fracPart % 10;   

    unsigned int intPart_prev = (int)STATE.previousTime ;        
    unsigned int fracPart_prev = (int)((STATE.previousTime  - intPart_prev) * 100);

    uint8_t d1_prev = intPart_prev / 10;    
    uint8_t d2_prev = intPart_prev % 10;    
    uint8_t d3_prev = fracPart_prev / 10;   
    uint8_t d4_prev = 0;       // fracPart_prev % 10;   // force this to be 0 so it is not drawn

    
    // DEBUG_PRINT("d1 is ");
    // DEBUG_PRINTLN(d1);
    // DEBUG_PRINT("d2 is ");
    // DEBUG_PRINTLN(d2);
    // DEBUG_PRINT("d3 is ");
    // DEBUG_PRINTLN(d3);
    // DEBUG_PRINT("d4 is ");
    // DEBUG_PRINTLN(d4);

    // DEBUG_PRINT("d1_prev is ");
    // DEBUG_PRINTLN(d1_prev);
    // DEBUG_PRINT("d2_prev is ");
    // DEBUG_PRINTLN(d2_prev);
    // DEBUG_PRINT("d3_prev is ");
    // DEBUG_PRINTLN(d3_prev);
    // DEBUG_PRINT("d4_prev is ");
    // DEBUG_PRINTLN(d4_prev);

    for(int row=0; row<8; row++)
    { 
      if(d1 !=  d1_prev  )
      {
        if(d1 == 0) lc.clearDisplay(3); 
        else { lc.setRow(3, row,  pgm_read_byte(&numbers[d1][row]));  }    // time left minutes tens
      }

      if(d2 != d2_prev) lc.setRow(2, row,  pgm_read_byte(&numbers[d2][row]));  // time left minutes ones
      if(d3 != d3_prev) lc.setRow(1, row,  pgm_read_byte(&numbers[d3][row]));  // time left seconds tens
      
      if(d4 != d4_prev) 
      {
        if(d4 == 0) lc.clearDisplay(0);
        else { lc.setRow(0, row,  pgm_read_byte(&numbers[d4][row])); }  // time left seconds ones
      }
    } 
    STATE.updateTimeThreeDigits = false;
    STATE.previousTime = STATE.timeLeft;
  }


  /////////////// DEBUG ///////////////// (temporary fix)

    int left_tens = SCORE.current_score_left/10;
    int left_ones = SCORE.current_score_left%10;
    int right_tens = SCORE.current_score_right/10;
    int right_ones = SCORE.current_score_right%10;
    for(uint8_t row = 0; row<8; row++)
    { 
      ls.setRow(1, row,  pgm_read_byte(&numbers[left_tens][row]));   // score tens left 
      ls.setRow(0, row, pgm_read_byte(& numbers[left_ones][row])); 

      rs.setRow(1, row,  pgm_read_byte(&numbers[right_tens][row]));
      rs.setRow(0, row,  pgm_read_byte(&numbers[right_ones][row])); 
    }

  ///////////////////// end debug ////////////////////////


  if ( (STATE.score_left_changed) || (STATE.score_right_changed) )
  {

    // update the score
    int left_tens = SCORE.current_score_left/10;
    int left_ones = SCORE.current_score_left%10;
    int right_tens = SCORE.current_score_right/10;
    int right_ones = SCORE.current_score_right%10;

    for(uint8_t row = 0; row<8; row++)
    { 
      // if changed, update the left score
      if(STATE.score_left_changed)
      {
        if ( (left_tens == 0) && (left_tens != (SCORE.previous_score_left/10) ) ) ls.clearDisplay(1);
        else if ( left_tens != (SCORE.previous_score_left/10) ) ls.setRow(1, row,  pgm_read_byte(&numbers[left_tens][row]));   // score tens left 
        if (left_ones != (SCORE.previous_score_left%10) ) ls.setRow(0, row, pgm_read_byte(& numbers[left_ones][row]));         // score ones left
      }
     if(STATE.score_right_changed)
      {
        if ( (right_tens == 0) && (right_tens != (SCORE.previous_score_right/10) ) ) rs.clearDisplay(1);
        else if ( right_tens != (SCORE.previous_score_right/10) ) rs.setRow(1, row,  pgm_read_byte(&numbers[right_tens][row]));   // score tens right 
        if (right_ones != (SCORE.previous_score_right%10) ) rs.setRow(0, row,  pgm_read_byte(&numbers[right_ones][row]));         // score ones right
      }
    }   
    SCORE.previous_score_left  = SCORE.current_score_left;
    SCORE.previous_score_right = SCORE.current_score_right;
  }

  







}