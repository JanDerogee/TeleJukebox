/*
  For Leonardo, Pro Micro and Due boards only, in combination with eh DFplayer mini MP3 player module

  Convert the classic rotary dial phone to a simple jukebox. Based on the "Wonderfoon" rpoject fromm Niek van Weeghel
  http://www.wonderfoon.nl/
  https://nos.nl/op3/artikel/2299833-niek-18-laat-dementerenden-bellen-met-hun-jeugdherinnering.html
  
  Allowing the user to dial a music-number on an old NON-modified PTT (T65) telefoon with rotary dial
*/

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"  /*<https://www.dfrobot.com/index.php?route=product/product&product_id=1121> <https://www.dfrobot.com/wiki/index.php/DFPlayer_Mini_SKU:DFR0299#Connection_Diagram> */
#include <avr/wdt.h>

/*IO-defintions*/
#define dbg_LED           6     /*simpe LED useful for all sorts of problems*/
#define mp3_player_busy   7     /*the pin of the MP3 player indicatingthat the device is busy*/
#define ph_hookpulse      8     /*the hook of the rotary dial phone*/
#define ph_button         9     /*the white button on the front of the phone*/

/*defines*/
#define DELAY_LONG        250   /*response delay, in ms*/
#define PULSE_TIMEOUTVAL  25    /*.. times 10msec*/


/*-----------------------------------------------------------------------*/

enum PlayerStates{MP3_IDLE,
                  MP3_PLAYING_PREVENT_CHANGE,
                  MP3_PLAYING_ALLOW_CHANGE,
                  MP3_RANDOM
                };
                
enum PhoneStates{ STATE_HOOK_DOWN,
                  STATE_HOOK_PICK_UP,
                  STATE_DIAL,
                  STATE_WAIT_FOR_INPUT
                };

/*do not change the order of this enum (unless you really know what you are doing)*/
enum PhoneEvents{ PHONE_DIAL_0,        /*0*/
                  PHONE_DIAL_1,        /*1*/
                  PHONE_DIAL_2,        /*2*/
                  PHONE_DIAL_3,        /*3*/
                  PHONE_DIAL_4,        /*4*/
                  PHONE_DIAL_5,        /*5*/
                  PHONE_DIAL_6,        /*6*/
                  PHONE_DIAL_7,        /*7*/
                  PHONE_DIAL_8,        /*8*/
                  PHONE_DIAL_9,        /*9*/
                  
                  PHONE_BUTTON,        /*user presses the white button on the front panel of the phone*/                  
                 
                  PHONE_OFF_HOOK,           /*the user took the handpiece of the hook, this event is generated only when the handpieces state goes from ON-HOOK to OFF-HOOK*/
                  PHONE_HANG_UP,            /*the user has put the handpiece back onto the hook, the phonecall is over*/
                  PHONE_TIMEOUT,            /*the user didn't do anything for a defined period of time*/
                  PHONE_IDLE
                };

/*-----------------------------------------------------------------------*/

DFRobotDFPlayerMini myDFPlayer;

/*routines*/
unsigned char PollPhone(void);
unsigned char CountDialPulses(void);
void PlayRandom(void);
unsigned char EasterEgg(String str);
void ErrorBlinky(unsigned char errorcode, unsigned char blinks_before_reset);
void Panic(void);

/*globals*/
unsigned char phone_state = STATE_HOOK_DOWN;   /*the statemachine of the phones behaviour*/

//unsigned char lp = 0;

/*-----------------------------------------------------------------------*/

void setup()
{
  pinMode(ph_button, INPUT_PULLUP);
  pinMode(ph_hookpulse, INPUT_PULLUP);
  pinMode(mp3_player_busy, INPUT);
  pinMode(dbg_LED, OUTPUT);

  digitalWrite(dbg_LED, HIGH);  /*light the LED during initialization*/

  Serial.begin(115200);     /*open a serial connection, for debugging purposes only (this is the virtual COM-port also used by the bootloader)*/
  Serial1.begin(9600);      /*open a serial connection, for debugging purposes only (this is the hardware serial)*/

  /*To make sure we miss nothing printed to the virtual COM-port, keep looping until the serial*/
  /*stream is opened (make some noise in the horn of the phone, to indicate that the system is in a loop)*/
//  while (!Serial) {ErrorBlinky(1, 0);} /*flash error code 1*/  /*Don't forget to comment out the line above for the final release !!!*/
  
  Serial.println("Rotary dial phone jukebox");
  Serial.println("Initializing DFPlayer...");

  //Use softwareSerial to communicate with mp3.
  if (!myDFPlayer.begin(Serial1, false, true))  /*no ack, but with reset*/
  {  
    Serial.println("Error: insert/check SD-card OR remove device from USB port of computer in order to use it");
    while(true) {ErrorBlinky(2, 0);} /*2=flash error code, 0=flash forever*/
  }
  Serial.println("DFPlayer Mini online.");

  delay(5000);  /*allow the SD-card to be initialized*/

  myDFPlayer.setTimeOut(500); //Set serial communication time out 500ms
  delay(250);  
  myDFPlayer.volume(30);                        /*Set volume value (0..30)*/
  delay(250);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);            /*----Set different EQ----*/
  delay(250);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);  /*----Set device we use SD as default----*/
  delay(250);

  //----Read imformation----
  Serial.print("MP3 state = "); 
  Serial.println(myDFPlayer.readState());
  delay(250);
  Serial.print("Volume = "); 
  Serial.println(myDFPlayer.readVolume());
  delay(250);
  Serial.print("Equalizer = "); 
  Serial.println(myDFPlayer.readEQ());
  delay(250);  

  digitalWrite(dbg_LED, LOW); /*done with initialization*/
}
  
/*-----------------------------------------------------------------------*/

void loop()
{
  unsigned char value = 0;
  static String dial_string = "";
  static unsigned char mp3_state = MP3_IDLE;   /*the statemachine of the phones behaviour*/

  if(phone_state != STATE_HOOK_DOWN)
  {
    if(digitalRead(mp3_player_busy) == 1) /*check if the MP3 player is in playback mode, because if not, then whatever was playing has finished, so allow playback of a new/different MP3*/  
    {
      if(mp3_state == MP3_RANDOM) /*when in random playback mode, play the next song*/
      {                           /*this way one press of the button can be usefull for hours of continuous music*/
        PlayRandom();             /*play a random file*/
        mp3_state = MP3_RANDOM;
      }
      else
      {      
        myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
        delay(200);                           /*allow some time to execute command*/
        myDFPlayer.playLargeFolder(1,2);      /*play from folder "01" the file "0002.MP3" containing the disconnected sound*/         
        mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
        delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/
      }
    }
  }
    
  value = PollPhone();
  switch(value)
  {
    case PHONE_OFF_HOOK:
    {
      dial_string = "";   /*reset the dial string*/
      myDFPlayer.stop();  /*stop playback of MP3 (although it should have stopped before we even got here)*/      
      delay(200);         /*delay to allow the stop request to be executed*/
      myDFPlayer.playLargeFolder(1,1);  /*play from folder "01" the file "0001.MP3" containing the dialtone sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/
      delay(200);      
      break;
    }

    case PHONE_HANG_UP:    
    {
      /*do something*/
      myDFPlayer.stop();  /*stop playback of MP3*/
      delay(200);         /*delay to allow the stop request to be executed*/      
      break;
    }    

    case PHONE_DIAL_0:  
    case PHONE_DIAL_1:
    case PHONE_DIAL_2:
    case PHONE_DIAL_3:
    case PHONE_DIAL_4:
    case PHONE_DIAL_5:
    case PHONE_DIAL_6:
    case PHONE_DIAL_7:
    case PHONE_DIAL_8:
    case PHONE_DIAL_9:
    {
      if(mp3_state != MP3_PLAYING_PREVENT_CHANGE)
      {
        mp3_state = MP3_PLAYING_PREVENT_CHANGE;       /*raise flag to prevent change of song during playback*/
        myDFPlayer.stop();                            /*stop any previous request, even if it stopped by itself, we need to send this*/
        delay(200);                                   /*allow for stopping to be executed*/
        myDFPlayer.playMp3Folder(value-PHONE_DIAL_0); /*play specific mp3 in SD:/MP3/0000.mp3 - 0009.mp3*/
        delay(200);                                   /*allow for the busy signal of the MP3 player to rise (we are going to detect this later, but we may not check too soon)*/
      }
      break;
    }

    case PHONE_BUTTON:        
    {     
      Serial.println(micros());
      randomSeed(micros());       /*initialize the random number generator, by a value that is based on the millis timer, this way (because the millis is determined by the user) it is much more random */
      PlayRandom();               /*play a random file*/
      mp3_state = MP3_RANDOM;      
      break;
    }
 
    case PHONE_IDLE:  /*when the phone is on the hook*/
    default:
    {
      break;
    }
  }

  /*if the user has dialed a number, then check if we meet the easter egg criteria*/
  if((value >= PHONE_DIAL_0) && (value <= PHONE_DIAL_9))
  {
    value = value - PHONE_DIAL_0;        /*remove the offset from the enum to get the value from 0-9*/
    dial_string = dial_string + value;
    EasterEgg(dial_string);
  }
}


/*-----------------------------------------------------------------*/


/*this routine will poll the phone for user input*/
/*it responds with an enumerated event value*/
unsigned char PollPhone(void)
{   
  unsigned char dial_val = 255;
  unsigned int tone_delay = 0;
  unsigned char ret_val = PHONE_IDLE;

  switch(phone_state)
  {   
    case STATE_HOOK_PICK_UP:
    {
      phone_state = STATE_WAIT_FOR_INPUT; /*further process button check/handling*/
      ret_val = PHONE_IDLE;                 /*nothing has happened yet, but the user is dialing*/      
      break;
    }     

    case STATE_DIAL:
    {
      dial_val = CountDialPulses();
      if(dial_val == 255)                 /*check for hang-up value*/
      {
        phone_state = STATE_HOOK_DOWN;    /*oops, the user aborted by hanging the handpiece up on the hook*/
        ret_val = PHONE_HANG_UP;
      }
      else
      {
        phone_state = STATE_WAIT_FOR_INPUT;
        ret_val = PHONE_DIAL_0 + dial_val;  /*convert the value of pulse_count to a value represented by the enum definition*/                              
      }      
      break;
    }

    case STATE_WAIT_FOR_INPUT:
    {
      if(digitalRead(ph_button) == 0)       /*check for button activity*/
      {
        delay(250);                             /*simply "wait" for bouncy contacts or jumpy fingers to stabilize*/        
        while(digitalRead(ph_button) == 0);     /*keep looping until released, produce a tone while pressing the button*/
        Serial.println("button detect");        
        phone_state = STATE_WAIT_FOR_INPUT;
        ret_val = PHONE_BUTTON;
        break;      
      }
      
      if(digitalRead(ph_hookpulse) == 1)    /*check for pulse activity*/
      {
        phone_state = STATE_DIAL;            /*go to the dial (pulse decoding) state*/
        ret_val = PHONE_IDLE;              
        break;
      }

      phone_state = STATE_WAIT_FOR_INPUT;            /*go to the dial (pulse decoding) state*/
      ret_val = PHONE_IDLE;              
      break;
    }

    case STATE_HOOK_DOWN:
    default:
    {
      if(digitalRead(ph_hookpulse) == 0)      /*check if hook is picked up*/
      {
        Serial.println("Hook is picked up");
        delay(DELAY_LONG);                    /*delay to wait out the noisy signals created by the hook switch*/
        phone_state = STATE_HOOK_PICK_UP;     /*new state is hook pick up*/
        ret_val = PHONE_OFF_HOOK;             /*signal this event to the caller of this routine*/
      }
      else
      {
        ret_val = PHONE_IDLE;
      }
      break;
    }
  }  

  return(ret_val);
}

/*this routine decodes the signal on the hook/pulse line and counts pulses or detects hang-up (receiver put back onto the hook)*/
unsigned char CountDialPulses(void)
{
  bool cur_pulse_state = HIGH;
  bool prev_pulse_state = HIGH; 
  unsigned char pulse_count = 0;
  unsigned char pulse_timeout = 0;
  
  pulse_timeout = PULSE_TIMEOUTVAL; /*reset timeout counter*/
  while(pulse_timeout > 0)  /*if the signal is too high for too long, then this isn't a valid pulse (user hangup or user pressing hookswitch)*/
  {
    prev_pulse_state = cur_pulse_state;
    cur_pulse_state = digitalRead(ph_hookpulse);       
    if((cur_pulse_state == LOW) && (prev_pulse_state == HIGH))        
    {
      pulse_count++;          
      pulse_timeout = PULSE_TIMEOUTVAL; /*reset timeout counter*/          
    }
    else
    {
      delay(10);
      pulse_timeout--;          
    }
  }

  if(digitalRead(ph_hookpulse) == LOW)  /*check current state of hook/pulse signal to determine whether we are hung up or done dialing*/
  {
    Serial.print("detected pulses=");
    Serial.print(pulse_count);
    Serial.print(" (user dialed a ");
    if(pulse_count > 9) {pulse_count = 0;};       /*when 10 pulses are counted this means the value 0 is dialed, se here we correct for that to get the correct numerical value*/
    Serial.print(pulse_count);
    Serial.println(")");
    return(pulse_count);
  }
  else
  {
    Serial.println("phone hung up");
    return(255);    /*255=error value for "hung up"*/
  }      
}

/*play a random file from the MP3 folder*/
void PlayRandom(void)
{
  unsigned int rnd = 0;
  
  Serial.println("Play random song");
  myDFPlayer.stop();  /*stop playback of MP3*/
  delay(200);
  rnd = random(0, 9); /*the min random value is 1, the max random value is 10 as there are 10 files in the MP3 folder*/
  Serial.print("rnd=");
  Serial.println(rnd);
  myDFPlayer.playMp3Folder(rnd);
  delay(200); /*allow for the busy signal of the MP3 player to rise (we are going to detect this later, but we may not check too soon)*/        
}

/*This routines check if the user has entered a dutch famous value for old telephones, number that were dialed many times before the dawn of the internet*/
/*When this number is dialed by this device,m an appropriate website is being shown*/
/*this routine return TRUE when an easter egg has been detected and executed (and FALSE if not)*/

unsigned char EasterEgg(String str)
{  
  Serial.print("str=");
  Serial.println(str);
  if(str == "002")  /*this is the old Dutch number for the time*/
  {
    Serial.print("002 = old Dutch number for the time");
    return(true); /*easter egg was detected and handled*/    
  }
  else
  if(str == "003")  /*this is the old Dutch number for the weather*/
  {
    Serial.print("003 = old Dutch number for the weather");
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "008")  /*this is the old Dutch number for telephone number information*/
  {
    Serial.print("008 = old Dutch number for telephone number information");
    return(true); /*easter egg was detected and handled*/
  }
  else
  {
    return(false);  /*no easter egg detected therefore do nothing and return FALSE*/
  }

}

/*-------------------------------------------------------*/

/*this routine flashes the LED to indicate an error number*/
/*make "blinks_before_reset" 0 and the routine will flash the code forever*/
/*make "blinks_before_reset" 1..255 and the routine will exit after flashing the code for that amount of times*/
void ErrorBlinky(unsigned char errorcode, unsigned char blinks_before_reset)
{
  unsigned char lp;
  
  while(1)
  { 
    for(lp=errorcode;lp>0;lp--)
    {
      digitalWrite(dbg_LED, HIGH);
      delay(500);
      digitalWrite(dbg_LED, LOW);
      delay(500);    
    }

    if(blinks_before_reset > 1)   {delay(2000); blinks_before_reset--;}
    if(blinks_before_reset == 1)  {break;}                /*exit the while loop*/   
  }  
}

/*this routine will reset the Arduino*/
void Panic(void)
{
    wdt_enable(WDTO_15MS);  /*turn on the WatchDog and wait for it to expire*/
    for(;;);
}    
