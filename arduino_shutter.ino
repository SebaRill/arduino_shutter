/*
 * !! USE AT YOUR OWN RISK !!
   
   Please read the comments bellow and take extra care with
   the relay logic. If you don't you WILL fry your shutter !!

   
   The purpose is obiously to open and close rolling shutters
   based on :
    - the buttons pressed next to each one (up and down)


   pushbutton applies in pushed mode +5V to pin
   10K resistor attached to pins from ground
   Each shutter is connected with 3 wires (up/down/phase), if the phase
   is connected to 'up' it will go up, if connected to 'down' it will go down.
   Up and Down should not be connected at the same time
   
   For each shutter, I have 2 push-like buttons ("go up" "go down").
   If up is pressed once, the shutter should go up
   If up is pressed a second time, the shutter should stop
   Same logic for down.
   
   Additionnally, you can set a timeout that will turn all relay off.



 */

#include <SPI.h>
#include <WString.h>
#include <EEPROM.h>


/*
   This is how my relay works
   could be the opposite on some other hardware
   Be extra CAREFULL here, because the "stop" state
   should be when all relays are openned.
   If all relays are closed at the same time, this could fry your shutter !
*/
  #define RELAY_CLOSED LOW
  #define RELAY_OPEN  HIGH
  
  #define BUTTON_PRESSED  HIGH
  #define BUTTON_RELEASED LOW
  
  // minimimun time in milliseconds between button pressed on one shutter
  // this prevent to switch relays too rapidly
  // (this can happen when input pins are not correctly
  // connected to the ground or vcc and occilate from the noise
  #define MIN_TIME_ACTION_MS 300
  
  // put back relays to stop state for the shutter, set to 0 to disable
  #define AUTO_STOP_TIMEOUT  65000

/*
 * how many shutters do you have ?
*/
const int nbmaxitems = 11;
const int nbshutters = 10;

/*
 * 
 *Connected pins on your arduino
 *ledUp and ledDown are for relays and pushButtonUp/pushButtonDown for buttons 
 *This is a [4 x nbmaxitems] matrix.
 *nbshutters columns should be filled, plus the last column for the centralized buttons
*/
int        ledUp[] =   {30, 32, 34, 36,  38,  40,  42,  44,  46,   48,    0 };
int        ledDown[] = {31, 33, 35, 37,  39,  41,  43,  45,  47,   49,    0 };
int pushButtonUp[] =   { 2,  5,  8, 11,  14,  16,  18,  22,  24,   26,   28 };
int pushButtonDown[] = { 3,  7,  9, 12,  15,  17,  19,  23,  25,   27,   29 };
String bufstr = String(100); //string for fetching data from address
unsigned long count = 0;

//////////////////////////
//  for button and relay
//////////////////////////
struct ITEM 
{ 
  int pin;
  boolean state;
};

//////////////////////////
// one SHUTTER has 2 buttons and 2 relays (up/down)
//////////////////////////
struct ROLLING_SHUTTER {
    ITEM buttons[2];
    ITEM relays[2];
    
    String strname;
    int last_action_button;
    
    unsigned long last_action_time_ms;
    
  };
  
//////////////////////////
// first (0) item is up
// and second (1) is down 
// this is used in the algorithm in order to find
// the "opposite" button or relay of 'up_down' by using '!up_down'
//////////////////////////
  #define ITEM_UP 0
  #define ITEM_DOWN 1

//////////////////////////
// actual data,
//////////////////////////
ROLLING_SHUTTER shutters[nbmaxitems];

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void trace_item(int index, int up_down, bool button)
{
      Serial.print("[");
      char tbs[16];
      sprintf(tbs, "%05d", count);
      Serial.print(tbs);
      Serial.print("]");
      Serial.print(button ? " button " : " relay  ");
      sprintf(tbs, "%02d", index);
      Serial.print(index);
      Serial.print(":");
      Serial.print(up_down == ITEM_UP ? "UP  " : "DOWN");
      Serial.print(" (");
      sprintf(tbs, "%02d", button ? shutters[index].buttons[up_down].pin : shutters[index].relays[up_down].pin);
      Serial.print(tbs);
      Serial.print(")");
      count++;
}

void trace_button(int index, int up_down)
{
      trace_item(index, up_down, true);
}

void trace_relay(int index, int up_down)
{
      trace_item(index, up_down, false);
}




void setup() {
  Serial.begin(9600);

  
  Serial.println("-- init --");
  
   
  char tbs[16];
  
  //init
  for(int i=0; i<nbmaxitems; i++)
  {
    delay(100); // wait for a second for stability
    shutters[i].buttons[ITEM_UP].pin = pushButtonUp[i];
    shutters[i].buttons[ITEM_UP].state = false;
    
    shutters[i].buttons[ITEM_DOWN].pin = pushButtonDown[i];
    shutters[i].buttons[ITEM_DOWN].state = false;
    
    shutters[i].relays[ITEM_UP].pin = ledUp[i];
    shutters[i].relays[ITEM_UP].state = false;
    
    shutters[i].relays[ITEM_DOWN].pin = ledDown[i];
    shutters[i].relays[ITEM_DOWN].state = false;
    
    shutters[i].last_action_button = -1;
    shutters[i].last_action_time_ms = millis();
    
    if(i >= nbshutters && i < nbmaxitems-1)
    {
       shutters[i].relays[ITEM_UP].pin = 0;
       shutters[i].relays[ITEM_DOWN].pin = 0;
       shutters[i].buttons[ITEM_UP].pin = 0;
       shutters[i].buttons[ITEM_DOWN].pin = 0;
       continue;
    }
   
     Serial.print("Shutter  ");
     sprintf(tbs, "[%02d] ", i);
     Serial.print(tbs);
    
    
    if( shutters[i].buttons[ITEM_UP].pin > 0)
    {
      pinMode (shutters[i].buttons[ITEM_UP].pin, INPUT);
    
      Serial.print(" BUTTON_UP ");
      sprintf(tbs, "(%02d),", shutters[i].buttons[ITEM_UP].pin);
      Serial.print(tbs);
      
      delay(100);
      
      pinMode (shutters[i].buttons[ITEM_DOWN].pin, INPUT);
     
      Serial.print(" BUTTON_DOWN ");
      sprintf(tbs, "(%02d),", shutters[i].buttons[ITEM_DOWN].pin);
      Serial.print(tbs);
      delay(100);
    }
    
    if(i != nbshutters && shutters[i].relays[ITEM_UP].pin > 0)
    {
      pinMode (shutters[i].relays[ITEM_UP].pin, OUTPUT);
      digitalWrite(shutters[i].relays[ITEM_UP].pin, RELAY_OPEN);
      Serial.print(" RELAY_UP ");
      sprintf(tbs, "(%02d),", shutters[i].relays[ITEM_UP].pin);
      Serial.print(tbs);
      delay(100);
      
      pinMode (shutters[i].relays[ITEM_DOWN].pin, OUTPUT);
      digitalWrite(shutters[i].relays[ITEM_DOWN].pin, RELAY_OPEN);
   
      Serial.print(" RELAY_DOWN ");
      sprintf(tbs, "(%02d),", shutters[i].relays[ITEM_DOWN].pin);
      Serial.print(tbs);
      
      delay(100);
    }
    
    Serial.println("");
  }
  Serial.print("auto timeout : ");
  Serial.print(AUTO_STOP_TIMEOUT);
  Serial.println(" ms");
  Serial.print("enable buttons : ");
  Serial.print("enable centralized button : ");
  Serial.println("-- init done --");
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void
ActivateRelay(int index, int up_down, int value)
{
   boolean previous_state = shutters[index].relays[up_down].state;
      
   digitalWrite(shutters[index].relays[up_down].pin, value);
   shutters[index].relays[up_down].state = (value == RELAY_CLOSED);
   
   if(previous_state != shutters[index].relays[up_down].state)
   {   
     trace_relay(index, up_down);
     Serial.print(" set to ");
     Serial.print(shutters[index].relays[up_down].state ? "CLOSED" : "OPEN" );
     Serial.println("");
   }
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
unsigned long millis_diff(unsigned long inow, unsigned long iref)
{
  if(inow >= iref)
  {
     return inow - iref;
  }
  
  return ((unsigned long)(-1) - iref) + inow;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void test_button(int index, int up_down)
{
  if(shutters[index].buttons[up_down].pin == 0)
    return;

  int current_val =   digitalRead(shutters[index].buttons[up_down].pin);
  int current_relay = digitalRead(shutters[index].relays[up_down].pin);
  //int current_relay_op = digitalRead(shutters[index].relays[!up_down].pin);
  int next_relay_val;
  unsigned long inow = millis();
  
  // timeout management
  if(   ( (AUTO_STOP_TIMEOUT) > 0 )
     && (current_relay == RELAY_CLOSED )
     )
  {
          unsigned long diff = millis_diff(inow, shutters[index].last_action_time_ms);
          
          if(diff  > (AUTO_STOP_TIMEOUT) )
          { 
            ActivateRelay(index, up_down, RELAY_OPEN);
            return;
          }
        
  }
  
  boolean current_state = (current_relay == RELAY_CLOSED );
  if( current_state != shutters[index].relays[up_down].state)
  {
     trace_relay(index, up_down);
     Serial.println("state mismatch !!");
     shutters[index].relays[up_down].state = current_state;
  }
  
  
  if (current_val == BUTTON_PRESSED)
  {

    if (shutters[index].last_action_button == -1 // first time (state is not correct)
        || shutters[index].last_action_button == (!up_down) // previous other button ?
        || !shutters[index].buttons[up_down].state)
    {       
      
      unsigned long diff = millis_diff(inow, shutters[index].last_action_time_ms);
      
        if(diff < MIN_TIME_ACTION_MS)
        {
           return;
        }
      
    
      trace_button(index, up_down);
      Serial.print(" pushed (");
      Serial.print(diff);
      Serial.println("ms)");
     
   // always disable opposite relay
     ActivateRelay(index, !up_down, RELAY_OPEN);
     
     bool bnext_state = !(current_relay == RELAY_CLOSED);
     next_relay_val = bnext_state ? RELAY_CLOSED :  RELAY_OPEN;
     
     ActivateRelay(index, up_down, next_relay_val);
     
     shutters[index].last_action_time_ms = inow;

    }

    shutters[index].buttons[up_down].state = true;
    shutters[index].last_action_button = up_down;
  }
  else if (current_val == BUTTON_RELEASED)
  {
     if( shutters[index].buttons[up_down].state)
     {
        trace_button(index, up_down);
        Serial.println(" RELEASED");
      }
      
    shutters[index].buttons[up_down].state = false;
  } 
  else
  {
      trace_button(index, up_down);
      Serial.println(" ??");
  }
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void
test_general_button(int index, int up_down)
{
  if(shutters[index].buttons[up_down].pin == 0)
    return;

  int current_val =   digitalRead(shutters[index].buttons[up_down].pin);
  int current_relay = shutters[index].relays[up_down].state ? RELAY_CLOSED : RELAY_OPEN; //digitalRead(shutters[index].relays[up_down].pin);
  int current_relay_op = shutters[index].relays[!up_down].state? RELAY_CLOSED : RELAY_OPEN; //digitalRead(shutters[index].relays[!up_down].pin);
  unsigned long inow = millis();
  
  // timeout management
  if(   ( (AUTO_STOP_TIMEOUT) > 0 )
     && (current_relay == RELAY_CLOSED )
     )
  {
          unsigned long diff = millis_diff(inow, shutters[index].last_action_time_ms);
          
          if(diff  > (AUTO_STOP_TIMEOUT) )
          {     
            shutters[index].relays[up_down].state = false;
            shutters[index].relays[!up_down].state = false;
            trace_relay(index, up_down);
            Serial.print(" auto timeout (");
            Serial.print(diff);
            Serial.println("ms)  ");
            
            return;
          }
        
  }
  
  
  if (current_val == BUTTON_PRESSED)
  {

    if (shutters[index].last_action_button == -1 
    ||  shutters[index].last_action_button == (!up_down)
    || (!shutters[index].buttons[up_down].state))
    {
      
      
    unsigned long diff = millis_diff(inow, shutters[index].last_action_time_ms);

      if(diff < MIN_TIME_ACTION_MS)
      {
          trace_button(index, up_down);
          Serial.print(" pushed too fast !! (");
          Serial.print(diff);
          Serial.println("ms)");
    
         return;
      }
    
    
      trace_button(index, up_down);
      Serial.print(" pushed (");
      Serial.print(diff);
      Serial.println("ms)");
      
      shutters[index].buttons[up_down].state = true;
   
      shutters[index].relays[up_down].state = !shutters[index].relays[up_down].state;
      shutters[index].relays[!up_down].state = false;
      
      for(int i=0; i<nbshutters; i++)
      {     
        shutters[i].relays[up_down].state = shutters[index].relays[up_down].state;
        shutters[i].relays[!up_down].state = false;
        
        digitalWrite (shutters[i].relays[!up_down].pin, RELAY_OPEN);
        digitalWrite (shutters[i].relays[up_down].pin, shutters[i].relays[up_down].state ? RELAY_CLOSED :  RELAY_OPEN);
      }
      
      shutters[index].last_action_time_ms = inow;
    }
   
    shutters[index].last_action_button = up_down;
  }
  else if (current_val == BUTTON_RELEASED)
  {
     if( shutters[index].buttons[up_down].state)
     {
        trace_button(index, up_down);
        Serial.println(" RELEASED");
      }
      
    shutters[index].buttons[up_down].state = false;
  }
  
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//function VR ALL
void vrall(int index)
{ 
  test_general_button(index, ITEM_UP);
  test_general_button(index, ITEM_DOWN);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//function VR
void vr(int index)
{
   test_button(index, ITEM_UP);
   test_button(index, ITEM_DOWN);
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void loop() {
  //
   for(int i=0; i<nbshutters; i++)
   {
         vr(i);
   }

   vrall(nbmaxitems-1); // -> VRALL ALL
  
  //delay(100);
  delay(100);
}
