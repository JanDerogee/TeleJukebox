/*
  For Leonardo, Pro Micro and Due boards only, in combination with eh DFplayer mini MP3 player module

  Convert the classic rotary dial phone to a simple jukebox. Based on the "Wonderfoon" rpoject fromm Niek van Weeghel
  http://www.wonderfoon.nl/
  https://nos.nl/op3/artikel/2299833-niek-18-laat-dementerenden-bellen-met-hun-jeugdherinnering.html
  
  Allowing the user to dial a music-number on an old NON-modified PTT (T65) telefoon with rotary dial


  note regarding used playroutines:
  ---------------------------------
  myDFPlayer.playFolder(15, 4);       //play specific mp3 in SD:/15/004.mp3; Folder Name(1~99); File Name(1~255)                    <-----== this is the most convenient method for us (because we have more then 10 folders but less then 255 files per folder)
  myDFPlayer.playMp3Folder(4);        //play specific mp3 in SD:/MP3/0004.mp3; File Name(0~65535)                                   <-- not suitable: having a folder named MP3 only makes things complicated, because all folders use MP3s
  myDFPlayer.playLargeFolder(2, 999); //play specific mp3 in SD:/02/004.mp3; Folder Name(1~10); File Name(1~1000)                   <-- not suitable: unusable, because the max. amount of useable folders is 10, we have 10 or more

*/

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"  /*<https://www.dfrobot.com/index.php?route=product/product&product_id=1121> <https://www.dfrobot.com/wiki/index.php/DFPlayer_Mini_SKU:DFR0299#Connection_Diagram> */
#include <avr/wdt.h>

#include <EEPROM.h>         /*use the EEPROM functionality of the 32u4 in the Arduino pro micro*/

/*-----------------------------------------------------------------------*/
/*                              IO-defintions                            */
/*.......................................................................*/

#define dbg_jmpr              2     /*simple jumper, when placed the firmware does something safe, so the bootloader can be operated without problems*/
#define dbg_LED               6     /*simpe LED useful for all sorts of problems*/
#define mp3_player_busy       7     /*the pin of the MP3 player indicatingthat the device is busy*/
#define ph_hookpulse_a        A8    /*this signal is processed as being analog for better noise suppression*/
#define ph_button             9     /*the white button on the front of the phone*/

/*defines*/
#define DELAY_LONG            250   /*response delay, in ms*/
#define PULSE_TIMEOUTVAL      25    /*.. times 10msec*/


#define DEFAULT_VOLUME        10     /*the volume setting has a default value of 10=100%*/
#define DEFAULT_MUSICFOLDER   1     /*the music folder that is used by default is folder 01*/
#define DEFAULT_RESERVED_1    5     /*reserved for future functionality*/
#define DEFAULT_RESERVED_2    5     /*reserved for future functionality*/
#define DEFAULT_EASTEREGG     1     /*the easter egg function is default 1=enabled*/

/*-----------------------------------------------------------------------*/

 /*this device has a few non-volatile settings which are stored in EEPROM*/
enum EEPROM_locations{ADDR_VOLUME,
                      ADDR_MUSICFOLDER,
                      ADDR_RESERVED_1,
                      ADDR_RESERVED_2,
                      ADDR_EASTEREGG
                     };

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
enum PhoneEvents{ PHONE_DIAL_0,                 /*0*/
                  PHONE_DIAL_1,                 /*1*/
                  PHONE_DIAL_2,                 /*2*/
                  PHONE_DIAL_3,                 /*3*/
                  PHONE_DIAL_4,                 /*4*/
                  PHONE_DIAL_5,                 /*5*/
                  PHONE_DIAL_6,                 /*6*/
                  PHONE_DIAL_7,                 /*7*/
                  PHONE_DIAL_8,                 /*8*/
                  PHONE_DIAL_9,                 /*9*/
                  
                  PHONE_BUTTON,                 /*user presses the white button on the front panel of the phone*/                  

                  PHONE_OFF_HOOK_AFTER_POWERON, /*the user took the handpiece of the hook for the first time since power-on/reset*/
                  PHONE_PICKUPWITHBUTTON,       /*the user took the handpiece of the hook while holding the button*/
                  PHONE_OFF_HOOK,               /*the user took the handpiece of the hook, this event is generated only when the handpieces state goes from ON-HOOK to OFF-HOOK*/
                  PHONE_HANG_UP,                /*the user has put the handpiece back onto the hook, the phonecall is over*/
                  PHONE_TIMEOUT,                /*the user didn't do anything for a defined period of time*/
                  PHONE_IDLE
                };

/*-----------------------------------------------------------------------*/
/*                                GLOBALS                                */
/*.......................................................................*/

int sett_volume       = DEFAULT_VOLUME;
int sett_mfolder      = DEFAULT_MUSICFOLDER;
int sett_reserved_1   = DEFAULT_RESERVED_1;
int sett_reserved_2   = DEFAULT_RESERVED_2;
int sett_easteregg    = DEFAULT_EASTEREGG;

unsigned char phone_state = STATE_HOOK_DOWN;    /*the statemachine of the phones behaviour*/
unsigned char mp3_state = MP3_IDLE;             /*the statemachine of the phones behaviour*/
unsigned char mp3_menu_state = 0;
unsigned long prev_millis = 0;                  /*value used by timeout timer*/

/*-----------------------------------------------------------------------*/
/*                                ROUTINES                               */
/*.......................................................................*/

DFRobotDFPlayerMini myDFPlayer;

/*routines*/
unsigned char PollPhone(void);
unsigned char CountDialPulses(void);
void SpeakCurrentSettings(void);
void ChangeSettings(void);
void PlayRandom(void);
void shuffle(int * t, int n);
unsigned char CheckSequence(String str);
void ErrorBlinky(unsigned char errorcode, unsigned char blinks_before_reset);
void Panic(void);
bool CheckHookPulseSignal(void);
void SetVolume(unsigned char vol);
void ReadAllSettings(void);
void WriteAllSettings(void);
void Timeout_Reset(void);
bool Timeout_Check(unsigned long timeout);

/*-----------------------------------------------------------------------*/
/*                            INTIALIZATION                              */
/*.......................................................................*/
void setup()
{
  pinMode(dbg_jmpr, INPUT_PULLUP);    /*this pin is mostly for debugging purposes, when this jumper is placed the entire system goes into a safe idle mode and does nothing but wait*/
  pinMode(ph_button, INPUT_PULLUP);
  pinMode(ph_hookpulse_a, INPUT);  
  pinMode(mp3_player_busy, INPUT);
  pinMode(dbg_LED, OUTPUT);

  digitalWrite(dbg_LED, HIGH);  /*light the LED during initialization*/

  Serial.begin(115200);     /*open a serial connection, for debugging purposes only (this is the virtual COM-port also used by the bootloader)*/
  Serial1.begin(9600);      /*open a serial connection to communicate with the MP3-player (this is the hardware serial)*/

  /*To make sure we miss nothing printed to the virtual COM-port, keep looping until the serial*/
  /*stream is opened (make some noise in the horn of the phone, to indicate that the system is in a loop)*/
  //while (!Serial);   /*this line is for debugging only, COMMENT THIS LINE OUT FOR THE FINAL RELEASE!!!*/
 
  Serial.println("Rotary dial phone jukebox");
  Serial.println("Initializing DFPlayer...");

  ReadAllSettings();  /*get all possible settings from EEPROM*/
 
  //Use softwareSerial to communicate with mp3.
  if (!myDFPlayer.begin(Serial1, false, true))  /*no ack, but with reset*/
  {  
    Serial.println("Error: insert/check SD-card OR remove device from USB port of computer in order to use it");
    while(true) {ErrorBlinky(2, 0);} /*2=flash error code, 0=flash forever*/
  }
  Serial.println("DFPlayer Mini online.");

  delay(5000);  /*allow the SD-card to be initialized*/

  myDFPlayer.setTimeOut(500); //Set serial communication time out 500ms
  SetVolume(sett_volume);

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
/*                                MAIN LOOP                              */
/*.......................................................................*/

void loop()
{
  unsigned char value = 0;
  static String dial_string = "";

  /*By using the dbg_jmpr line (make it low) we can enter a safe loop, where nothing exiting or unexpected happens, this way we have a safe place to go to when there are USB/bootloader problems*/
  /*this safe place is only needed when I (the programmer) screw up and make some sort of endless loop (which eats all CPU time and therefore starves the USB routines (which then fail to work).*/
  if(digitalRead(dbg_jmpr) == LOW) /*check if the debug jumper is placed, if so we do something innocent, this way we have an escape in case the code below is screwing up the bootloader (this functionality is mainly important during development)*/  
  {
    Serial.println("Debug mode active");    
    digitalWrite(dbg_LED, LOW);
    delay(250);      
    digitalWrite(dbg_LED, HIGH);
    delay(500);  
  }
  else  /*normal operation*/
  {
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
          myDFPlayer.playFolder(90,2);          /*play from folder .. the file ".....MP3" containing the disconnected sound*/         
          mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
          delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/
        }
      }
    }
      
    value = PollPhone();
    switch(value)
    {
      case PHONE_PICKUPWITHBUTTON:
      {     
        ChangeSettings();                     /*when the phone is picked up while holding the button down then start the configuration menu*/
        break;
      }
           
      case PHONE_OFF_HOOK_AFTER_POWERON:
      {
        SpeakCurrentSettings();               /*speak the current settings*/
        break;
      }
      
      case PHONE_OFF_HOOK:
      {
        dial_string = "";                     /*reset the dial string*/
        myDFPlayer.stop();                    /*stop playback of MP3 (although it should have stopped before we even got here)*/      
        delay(200);                           /*delay to allow the stop request to be executed*/
        myDFPlayer.playFolder(90,1);          /*play from folder .. the file ".....MP3" containing the dialtone sound*/         
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
        value = value - PHONE_DIAL_0;                       /*remove the offset from the enum to get the value from 0-9*/
        if (Timeout_Check(5000) == true)
        {
          dial_string=value;                                /*clear the previous sequence and add latest value*/
          Serial.print("dial_string has expired, new dial_string=");          
          Serial.println(dial_string);          
        }
        else
        {       
          dial_string = dial_string + value;                /*update the dial string with the latest value*/
          Serial.print("new dial_string=");          
          Serial.println(dial_string);
        }
        Timeout_Reset();                                    /*reset the timeout that can make the dial_string expire*/
        
        if(CheckSequence(dial_string) == true)     /*if the user has dialed a number, then first check if we have dialed a special sequence of numbers that have a special function*/
        {
          dial_string="";                                   /*clear the sequence to allow a new sequence to be detected*/
        }
        else                                      /*if it wasn't a sequence, then just play an MP3 as indicated by the number*/
        {     
          if(mp3_state != MP3_PLAYING_PREVENT_CHANGE)
          {
            mp3_state = MP3_PLAYING_PREVENT_CHANGE;         /*raise flag to prevent change of song during playback*/
            myDFPlayer.stop();                              /*stop any previous request, even if it stopped by itself, we need to send this*/
            delay(200);                                     /*allow for stopping to be executed*/
            if(value == 0)                                  /*check if a 0 was dialed*/
            {                                               /*because if so we need to play 010.MP3 (and not 000.MP3, because the player doesn't allow that)*/
              myDFPlayer.playFolder(sett_mfolder, 10);      /*play mp3 from the currently selected folder*/
            }
            else
            {
              myDFPlayer.playFolder(sett_mfolder, value);   /*play mp3 from the currently selected folder*/
            }
            delay(200);                                     /*allow for the busy signal of the MP3 player to rise (we are going to detect this later, but we may not check too soon)*/
          }
        }
        break;
      }
  
      case PHONE_BUTTON:        
      {     
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
    
  }
}


/*-----------------------------------------------------------------*/


/*this routine will poll the phone for user input*/
/*it responds with an enumerated event value*/
unsigned char PollPhone(void)
{ 
  static bool system_powerup = true;  /*this flag is used to keep track of the fact if the system has been powered up recently*/ 

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
      if(digitalRead(ph_button) == 0)           /*check for button activity*/
      {
        delay(250);                             /*simply "wait" for bouncy contacts or jumpy fingers to stabilize*/        
        while(digitalRead(ph_button) == 0)      /*keep looping until released*/
        {
          delay(1);
        }
        
        Serial.println("button detect");        
        phone_state = STATE_WAIT_FOR_INPUT;
        ret_val = PHONE_BUTTON;
        break;      
      }
      
      if(CheckHookPulseSignal() == HIGH)        /*check for pulse activity*/
      {
        phone_state = STATE_DIAL;               /*go to the dial (pulse decoding) state*/
        ret_val = PHONE_IDLE;              
        break;
      }

      phone_state = STATE_WAIT_FOR_INPUT;       /*go to the dial (pulse decoding) state*/
      ret_val = PHONE_IDLE;              
      break;
    }

    case STATE_HOOK_DOWN:
    default:
    {
      if(CheckHookPulseSignal() == LOW)         /*check if hook is picked up*/
      {
        Serial.println("Hook is picked up");
        delay(DELAY_LONG);                      /*delay to wait out the noisy signals created by the hook switch*/
        phone_state = STATE_HOOK_PICK_UP;       /*new state is hook pick up*/
        if(digitalRead(ph_button) == 0) {ret_val=PHONE_PICKUPWITHBUTTON;}       /*check if button is pressed while the phone is being picked up from it's hook*/
        else if(system_powerup == true) {ret_val=PHONE_OFF_HOOK_AFTER_POWERON;} /*just a regular off hook, defenitely not the first time since power-on/reset*/
        else                            {ret_val=PHONE_OFF_HOOK;}               /*signal this event to the caller of this routine*/
        system_powerup=false; /*clear the flag that indicates the firts hoop pickup since power-on/reset*/
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
    cur_pulse_state = CheckHookPulseSignal();       
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

  if(CheckHookPulseSignal() == LOW)   /*check current state of hook/pulse signal to determine whether we are hung up or done dialing*/
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
  static int playlist_cnt = 10;                     /*the default value is 10, this means that the playlist will be scranbled upon the first call of the PlayRondom() function*/
  static int playlist[10] = {1,2,3,4,5,6,7,8,9,10};

  if(playlist_cnt >= 10)
  {
    playlist_cnt = 0;
    shuffle(playlist, 10); /*shuffle contents of the playlist array (which has a size of 10 entries)*/
  }
   
  Serial.print("Play random, song ");
  Serial.println(playlist[playlist_cnt]);  
  myDFPlayer.stop();  /*stop playback of MP3*/
  delay(200); 
  myDFPlayer.playFolder(sett_mfolder, playlist[playlist_cnt]);
  playlist_cnt++;
  delay(200); /*allow for the busy signal of the MP3 player to rise (we are going to detect this later, but we may not check too soon)*/        
}

/*This routine shuffles an array, it is a very practical way of creating a random order playlist that prevents repeating a song before all other songs are played*/
/*the way it works is best explained here: http://en.wikipedia.org/wiki/Fisher-Yates_shuffle */
void shuffle(int * t, int n)
{
  unsigned int rnd = 0;
  int array_size = 0;
  int temp = 0;
  int lp = 0;

  randomSeed(micros());       /*initialize the random number generator, by a value that is based on the millis timer, this way (because the millis is determined by the user) it is much more random */
  array_size = n;   /*save array size value to another variable, because we are about the destroy the contents of n*/
  while(--n >= 2)
  {
    rnd = random(0, n); /*the min random value is 1, the max random value is 10 as there are 10 files in the MP3 folder*/
    temp = t [n];
    t[n] = t[rnd];
    t[rnd] = temp;
  }

//  /*debug print the new array contents*/
//  for(lp=0; lp<array_size; lp++)
//  {
//    Serial.print(t[lp]);
//    Serial.println(",");
//  }        
}
 
 

/*This routines check if the user has entered a dutch famous value for old telephones, number that were dialed many times before the dawn of the internet*/
/*When this number is dialed by this device,m an appropriate website is being shown*/
/*this routine return TRUE when an easter egg has been detected and executed (and FALSE if not)*/
unsigned char CheckSequence(String str)
{
  unsigned char return_value = false;    /*no sequence was detected*/
    
  Serial.print("str=");
  Serial.println(str);

  /*special number sequence handling*/
  if((str == "911") || (str == "112") || (str == "09008844") || (str == "999") || (str == "01189998819991197253"))    /*Check for emergency number, if so, warn user that this phone cannot be used*/
  {
    Serial.println("You've dialed an alarm related number");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playFolder(90,4);          /*play from folder ... the file ".....MP3" containing the related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return_value = true;                  /*sequence was detected and handled*/
  }
  else
  if(str == "738")                        /*Check for "settings" number. This allows users, without a frontside button on their phone, to enter the configuration menu*/
  {
    Serial.println("You've dialed the number for a settings request");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/    
    ChangeSettings();                     /*when the phone is picked up while holding the button down then start the configuration menu*/    
    return_value = true;                  /*sequence was detected and handled*/
  }


  /*easter egg handling*/
  if(sett_easteregg > 0)
  { 
    if(str == "002")  /*this is the old Dutch number for the current time*/
    {
      Serial.print("002 = old Dutch number for the current time");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,2);   /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
      return_value = true;                  /*sequence was detected and handled*/
    }
    else
    if(str == "003")  /*this is the old Dutch number for the weather*/
    {
      Serial.print("003 = old Dutch number for the weather");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,3);   /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
      return_value = true;                  /*sequence was detected and handled*/
    }
    else
    if(str == "008")  /*this is the old Dutch number for telephone number information*/
    {
      Serial.print("008 = old dutch number for telephone number related information");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,8);      /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
      return_value = true;                  /*sequence was detected and handled*/
    }
    else
    if(str == "1945")  /*this is the year of birth of Deborah Harry (a.k.a. Blondie), she mad a sone about a telephone*/
    {
      Serial.print("1945 = year of birth of Deborah Harry (a.k.a. Blondie)");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,45);   /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/        
      return_value = true;                  /*sequence was detected and handled*/
    }
    else
    if(str == "1947")  /*this is the year of birth of one of the greatest dutch commedians (Andre van Duin), he made a funny song about a telephone*/
    {
      Serial.print("1947 = year of birth of Adrianus Marinus Kyvon (a.k.a. Andre van Duin)");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,47);   /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/        
      return_value = true;                  /*sequence was detected and handled*/
    }
    else
    if(str == "1950")  /*Stevie Wonder was born in 1950, he made a song about a telephone call*/
    {
      Serial.print("1950 = year of birth of Stevie Wonder");
      myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
      delay(200);                           /*allow some time to execute command*/
       myDFPlayer.playFolder(99,50);   /*play from folder .. the file ".....MP3" containing the easter egge related sound*/         
      mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
      delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
      return_value = true;                  /*sequence was detected and handled*/
    }  
  }
  
  return(return_value);
}

/*-------------------------------------------------------*/

/*this routine "speaks" the current settings*/
void SpeakCurrentSettings(void)
{
  unsigned char scs_exit = false;
  unsigned char scs_state = 0;
  
  Serial.println("Speak the settings");
  delay(1500);              /*allow some time for the user to hold the handpiece to their ear*/
  scs_exit = false;         /*make sure we don't exit prematurely*/
  scs_state = 0;            /*make sure we start the statemachine from the beginning*/
  while(scs_exit == false)
  {
    myDFPlayer.stop();                      /*song has ended, play the disconnected sound*/
    delay(200);                             /*allow some time to execute command*/   
    switch(scs_state)
    {
      case 0:   {myDFPlayer.playFolder(90,100);                             scs_exit = false;   break;}   /*introduction to telephone and settings*/
      case 1:   {myDFPlayer.playFolder(90,104);                             scs_exit = false;   break;}   /*volume*/
      case 2:   {myDFPlayer.playFolder(90,(20+sett_volume));                scs_exit = false;   break;}   /*volume value in ..%*/
      case 3:   {myDFPlayer.playFolder(90,105);                             scs_exit = false;   break;}   /*music folder*/
      case 4:   {myDFPlayer.playFolder(90,(10+sett_mfolder));           scs_exit = false;   break;}   /*folder number*/
      case 5:   {myDFPlayer.playFolder(90,108);                             scs_exit = false;   break;}   /*easter egg function*/
      case 6:   {if(sett_easteregg == false)  {myDFPlayer.playFolder(90,7); scs_exit = true;    break;}   /*disabled*/    
                 else                         {myDFPlayer.playFolder(90,8); scs_exit = true;    break;}}  /*enabled*/    
      default:  {                                                           scs_exit = true;    break;}   /*if we ever get here... do nothing and exit menu handler*/
    }
    scs_state++;                              /*increment state counter*/
    delay(300);                               /*allow some time to execute command*/
    while(digitalRead(mp3_player_busy) == 0)  /*check if the MP3 player is in playback mode, because if not, then whatever was playing has finished, so allow playback of a new/different MP3*/  
    {
      delay(100);
      if(PollPhone() == PHONE_HANG_UP)
      {
        Serial.println("Speaking of setings, has been terminated");
        myDFPlayer.stop();          /*stop playback of MP3*/        
        delay(200);                 /*delay to allow the stop request to be executed*/      
        scs_exit = true;            /*this stops the speech statemachine*/
        break;
      }
    }
  }
}

/*this routine "speaks" the current settings*/
void ChangeSettings(void)
{
  unsigned char lp_exit = false;
  unsigned char cs_exit = false;
  unsigned char cs_state = 0;
  unsigned char value = 0;
  unsigned char allow_input = false;
 
  Serial.println("Menu started...");
  SetVolume(10);           /*use the maximum possible volume, this way the user can always hear the menu, even if the true volume settings is at it's lowest*/
  delay(1000);             /*allow some time for the user to hold the handpiece to their ear*/
  cs_exit = false;         /*make sure we don't exit prematurely*/
  cs_state = 0;            /*make sure we start the statemachine from the beginning*/
  while(cs_exit == false)
  {   
    //Serial.print("cs_state=");
    //Serial.println(cs_state);
    switch(cs_state)
    { /*main menu start*/
      case 0:   {cs_exit = false;  cs_state=1;   break;}   /*this does nothing other then making sure all previous MP3 are stopped*/
      case 1:   {myDFPlayer.playFolder(90,101);               delay(200); cs_exit=false;  cs_state=3;   allow_input=false;  break;}   /*introduction to menu and request the user to make a choice*/
      case 2:   {myDFPlayer.playFolder(90,102);               delay(200); cs_exit=false;  cs_state=3;   allow_input=false;  break;}   /*request the user to make a choice*/
      case 3:   {myDFPlayer.playFolder(90,9);                 delay(200); cs_exit=false;  cs_state=4;   allow_input=true;   break;}   /*play 10 second silence, this allows the user to make a choice*/
      case 4:   {switch(value)
                  {
                    case 1:   {cs_exit = false;  cs_state=10;  break;}   /*valid input detected*/
                    case 2:   {cs_exit = false;  cs_state=20;  break;}   /*valid input detected*/
                    case 3:   {cs_exit = false;  cs_state=30;  break;}   /*valid input detected*/
                    case 255: {cs_exit = false;  cs_state=2;   break;}   /*keep waiting for input*/
                    default:  {cs_exit = false;  cs_state=5;   break;}   /*invalid input, inform user*/
                  }
                  break;
                }
      case 5:   {myDFPlayer.playFolder(90,6);                 delay(200); cs_exit=false;  cs_state=1;   allow_input=false;  break;}   /*invalid menu choice, indicate problem and go back to first state*/
      
      /*volume menu*/
      case 10:  {myDFPlayer.playFolder(90,110);               delay(200); cs_exit=false;  cs_state=11;  allow_input=false;  break;}   /*confirm choice*/
      case 11:  {myDFPlayer.playFolder(90,111);               delay(200); cs_exit=false;  cs_state=12;  allow_input=false;  break;}   /*speak current setting name*/
      case 12:  {myDFPlayer.playFolder(90,(20+sett_volume));  delay(200); cs_exit=false;  cs_state=13;  allow_input=false;  break;}   /*speak current setting value*/      
      case 13:  {myDFPlayer.playFolder(90,102);               delay(200); cs_exit=false;  cs_state=14;  allow_input=false;  break;}   /*request to make a choice*/
      case 14:  {myDFPlayer.playFolder(90,9);                 delay(200); cs_exit=false;  cs_state=15;  allow_input=true;   break;}   /*play 10 second silence, this allows the user to make a choice*/
      case 15:  {switch(value)
                  {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                    case 10:  {sett_volume = value; cs_exit=false;  cs_state=16;  break;}
                    case 255: {cs_exit = false;     cs_state=13;  break;}   /*keep waiting for input*/
                    default:  {cs_exit = false;     cs_state=15;  break;}   /*invalid input, inform user*/
                  }
                  break;
                }
      case 16:  {WriteAllSettings();
                 myDFPlayer.playFolder(90,103);               delay(200); cs_exit=false;  cs_state=17;  allow_input=false;  break;}   /*confirm choice*/               
      case 17:  {myDFPlayer.playFolder(90,(20+sett_volume));  delay(200); cs_exit=false;  cs_state=99;  allow_input=false;  break;}   /*speak current setting value*/      

      /*music folder menu*/
      case 20:  {myDFPlayer.playFolder(90,120);               delay(200); cs_exit=false;  cs_state=21;  allow_input=false;  break;}   /*confirm choice*/
      case 21:  {myDFPlayer.playFolder(90,121);               delay(200); cs_exit=false;  cs_state=22;  allow_input=false;  break;}   /*speak current setting name*/
      case 22:  {myDFPlayer.playFolder(90,(10+sett_mfolder)); delay(200); cs_exit=false;  cs_state=23;  allow_input=false;  break;}   /*speak current setting value*/      
      case 23:  {myDFPlayer.playFolder(90,102);               delay(200); cs_exit=false;  cs_state=24;  allow_input=false;  break;}   /*request to make a choice*/
      case 24:  {myDFPlayer.playFolder(90,9);                 delay(200); cs_exit=false;  cs_state=25;  allow_input=true;   break;}   /*play 10 second silence, this allows the user to make a choice*/
      case 25:  {switch(value)
                  {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                    case 10:  {sett_mfolder = value;  cs_exit=false;  cs_state=26;  break;}
                    case 255: {cs_exit = false;       cs_state=23;  break;}   /*keep waiting for input*/
                    default:  {cs_exit = false;       cs_state=25;  break;}   /*invalid input, inform user*/
                  }
                  break;
                }
      case 26:  {WriteAllSettings();
                 myDFPlayer.playFolder(90,103);               delay(200); cs_exit=false;  cs_state=27;  allow_input=false;  break;}   /*confirm choice*/               
      case 27:  {myDFPlayer.playFolder(90,(10+sett_mfolder)); delay(200); cs_exit=false;  cs_state=99;  allow_input=false;  break;}   /*speak current setting value*/      

      /*easter egg menu*/
      case 30:  {myDFPlayer.playFolder(90,130);               delay(200); cs_exit=false;  cs_state=31;  allow_input=false;  break;}   /*confirm choice*/
      case 31:  {myDFPlayer.playFolder(90,131);               delay(200); cs_exit=false;  cs_state=32;  allow_input=false;  break;}   /*speak current setting name*/
      case 32:  {if(sett_easteregg == false)  {myDFPlayer.playFolder(90,7); delay(200); cs_exit=false;  cs_state=33;  allow_input=false;  break;}   /*disabled*/    
                 else                         {myDFPlayer.playFolder(90,8); delay(200); cs_exit=false;  cs_state=33;  allow_input=false;  break;}}  /*enabled*/                     
      case 33:  {myDFPlayer.playFolder(90,102);               delay(200); cs_exit=false;  cs_state=34;  allow_input=false;  break;}   /*request to make a choice*/
      case 34:  {myDFPlayer.playFolder(90,9);                 delay(200); cs_exit=false;  cs_state=35;  allow_input=true;   break;}   /*play 10 second silence, this allows the user to make a choice*/
      case 35:  {switch(value)
                  {
                    case 10:  {sett_easteregg=false;  cs_exit=false;  cs_state=37;  break;}                    
                    case 1:   {sett_easteregg=true;   cs_exit=false;  cs_state=37;  break;}
                    case 255: {cs_exit = false;       cs_state=33;  break;}   /*keep waiting for input*/
                    default:  {cs_exit = false;       cs_state=36;  break;}   /*invalid input, inform user*/
                  }
                  break;
                }
      case 36:  {WriteAllSettings();
                 myDFPlayer.playFolder(90,6);                 delay(200); cs_exit=false;  cs_state=30;  allow_input=false;  break;}  /*invalid menu choice, indicate problem and go back to first state*/
      case 37:  {myDFPlayer.playFolder(90,103);               delay(200); cs_exit=false;  cs_state=38;  allow_input=false;  break;}   /*confirm choice*/            
      case 38:  {if(sett_easteregg == false)  {myDFPlayer.playFolder(90,7); delay(200); cs_exit=false;  cs_state=99;  allow_input=false;  break;}   /*disabled*/    
                 else                         {myDFPlayer.playFolder(90,8); delay(200); cs_exit=false;  cs_state=99;  allow_input=false;  break;}}  /*enabled*/            

      /*main menu end*/
      case 99:  {myDFPlayer.playFolder(90,109);               delay(200); cs_exit=false;  cs_state=100; allow_input=false;  break;}   /*indicate that the new setting is saved*/      
      case 100: {myDFPlayer.playFolder(90,9);                 delay(200); cs_exit=false;  cs_state=99;  allow_input=false;  break;}   /*play 10 second silence, this allows the user to make a choice*/      
      default:  {cs_exit=true;  break;}   /*if we ever get here... do nothing and exit menu handler*/
    }

    lp_exit = false;    /*by default we don't exit the sub loop*/
    while((digitalRead(mp3_player_busy)==0) && (lp_exit == false))  /*check if the MP3 player is in playback mode, because if not, then whatever was playing has finished, so allow playback of a new/different MP3*/  
    {
      delay(10);            /*prevent this while loop from running too fast*/
      switch(PollPhone())
      {
        default:            {value=255; cs_exit=false;  lp_exit=false;        break;} /*do nothing*/        
        case PHONE_DIAL_1:  {value=1;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_2:  {value=2;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_3:  {value=3;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_4:  {value=4;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_5:  {value=5;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_6:  {value=6;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_7:  {value=7;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_8:  {value=8;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_9:  {value=9;   cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/
        case PHONE_DIAL_0:  {value=10;  cs_exit=false;  lp_exit=allow_input;  break;} /*process the entered value*/

        case PHONE_HANG_UP:
        {
          Serial.println("Menu has been terminated");
          cs_exit =true;      /*this stops the speech statemachine*/
          lp_exit =true;      /*exit this while loop*/
          break;
        }       
      }
    }

    myDFPlayer.stop();       /*make sure nothing else is playing*/
    delay(200);              /*allow some time to execute command*/       
      
  }
  SetVolume(sett_volume); /*use the true volume as defined by the volume setting*/
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
    if(blinks_before_reset == 1)  {break;}                                /*exit the while loop*/   
  }  
}

/*this routine will reset the Arduino*/
void Panic(void)
{
    wdt_enable(WDTO_15MS);  /*turn on the WatchDog and wait for it to expire*/
    for(;;);
}    

/*This routine samples the input and checks the value in relation to the previous value in order to determine if it is high or low*/
/*You would expect this to be done with a simple DigitalRead(..) function. But unfortunately, the trigger levels for high and low*/
/*on the 32U4 are very low and have no usefull hysteresis for the noisy signal we are trying to read*/
/*So this routine converts the signal from the analog domain and uses that value to determine wether it is high or low, this time with a very large hysteresis*/
/*this way we are less susceptable to noise that may be possible on the signal we are trying to read*/
bool CheckHookPulseSignal(void)
{
  int adc_val = 0;    /*variable to store the value coming from the sensor (this value ranges from 0 to 1023)*/

  static int  filter_cnt = 0;
  static bool current_level = 0;
  static bool previous_level = 0;

  const int centervalue = 512;  /*centervalue is halve of the full range of the ADC swing (which is 1024 steps)*/
  const int hysteresis = 256;
  const int filter_thr = 3;

  
  adc_val = analogRead(ph_hookpulse_a);
  if(previous_level == LOW)
  {
    if(adc_val > (centervalue + hysteresis)){current_level = 1;}
    else                                    {current_level = 0;}
  }
  else
  {
    if(adc_val > (centervalue - hysteresis)){current_level = 1;}
    else                                    {current_level = 0;}    
  }
  
  /*the line below is for debugging only*/
 // Serial.println(sensorValue);  delay(1000); /*delay is required, because constant printing will prevent the USB functionality (and therefore the bootloader) from operating reliably*/
  

  previous_level = current_level;
  digitalWrite(dbg_LED, !current_level);  /*the LED represents the state of the phone, when it is oof, the phone is on hook, when it is ON, the phone is picked up, when it blinks, the user is dialing a number*/
  
  return(current_level);  
}

void SetVolume(unsigned char vol)
{
  delay(250);   /*small delay, to make sure any previous command has been handled before we sent a new one*/
  switch(vol)
  {
    case 1: {myDFPlayer.volume(14); break;}  /*3 sets volume to a true (linear) 10%, but linear volume is too soft, too fast, so we just use a more practical heavily simplified scale*/
    case 2: {myDFPlayer.volume(16); break;}  /*6 sets volume to 20%*/
    case 3: {myDFPlayer.volume(18); break;}  /*9 sets volume to 30%*/    
    case 4: {myDFPlayer.volume(20); break;} /*12 sets volume to 40%*/
    case 5: {myDFPlayer.volume(22); break;} /*15 sets volume to 50%*/
    case 6: {myDFPlayer.volume(24); break;} /*18 sets volume to 60%*/    
    case 7: {myDFPlayer.volume(26); break;} /*21 sets volume to 70%*/
    case 8: {myDFPlayer.volume(28); break;} /*24 sets volume to 80%*/
    case 9: {myDFPlayer.volume(29); break;} /*27 sets volume to 90%*/    
    case 10:{myDFPlayer.volume(30); break;} /*30 sets volume to 100%*/        
    default:{myDFPlayer.volume(30); break;} /*30 sets volume to 100% (it's the best we can do for an unknown value*/        
  }
}


/*This routine will read the settings from EEPROM, if the value is outside the expected range it will be set to default*/
/*This means that the system will automatically be set to default directly after programming*/
/*Because the value we are storing are never 0 and never 255, we can easily distinguish them from an unprogrammed EEPROM and therefore detect if they are valid*/
void ReadAllSettings(void)
{
  Serial.println("Reading settings from EEPROM");
  sett_volume = EEPROM.read(ADDR_VOLUME);
  if((sett_volume<1) || (sett_volume>10)) /*check if the value read from EEPROM is within the expected range, if not replace value by default value*/
  {
    Serial.println("Volume setting has been set to default"); 
    sett_volume = DEFAULT_VOLUME;
  }
  
  sett_mfolder = EEPROM.read(ADDR_MUSICFOLDER);
  if((sett_mfolder<1) || (sett_mfolder>10)) /*check if the value read from EEPROM is within the expected range, if not replace value by default value*/
  {
    Serial.println("Music folder setting has been set to default"); 
    sett_mfolder = DEFAULT_MUSICFOLDER;
  }
  
  sett_reserved_1 = EEPROM.read(ADDR_RESERVED_1);
  if((sett_reserved_1<1) || (sett_reserved_1>10)) /*check if the value read from EEPROM is within the expected range, if not replace value by default value*/
  {
    Serial.println("Reserved_1 setting has been set to default"); 
    sett_reserved_1 = DEFAULT_RESERVED_1;
  }
  
  sett_reserved_2 = EEPROM.read(ADDR_RESERVED_2);
  if((sett_reserved_2<1) || (sett_reserved_2>10)) /*check if the value read from EEPROM is within the expected range, if not replace value by default value*/
  {
    Serial.println("Reserved_2 setting has been set to default"); 
    sett_reserved_2 = DEFAULT_RESERVED_2;
  }
  
  sett_easteregg = EEPROM.read(ADDR_EASTEREGG) - 128; /*we use an offset, just to make sure that a value of 0 will not be stored in the EEPROM*/
  if((sett_easteregg<0) || (sett_easteregg>1))        /*check if the value read from EEPROM is within the expected range, if not replace value by default value*/
  {
    Serial.println("Easter egg setting has been set to default"); 
    sett_easteregg = DEFAULT_EASTEREGG;
  }  
}

/*this routine will write all setting to EEPROM*/
/*now this may not seem very efficient, to write all values if maybe only one value is changed*/
/*but since time is not an issue and the number of writes for the lifespan of the device s not an issue*/
/*we just save all at once, because it keeps the code very simple*/
void WriteAllSettings(void)
{
  Serial.println("Saving settings to EEPROM");
  EEPROM.write(ADDR_VOLUME, sett_volume);
  EEPROM.write(ADDR_MUSICFOLDER, sett_mfolder);
  EEPROM.write(ADDR_RESERVED_1, sett_reserved_1);
  EEPROM.write(ADDR_RESERVED_2, sett_reserved_2);
  EEPROM.write(ADDR_EASTEREGG, (sett_easteregg+128));  /*add an offset, just to make sure that we don't store the value 0 in EEPROM*/
}


/*this routine will reset the timeout counter*/
void Timeout_Reset(void)
{
  prev_millis = millis(); /*update timeout counter with the current time*/
}

/*this routine will compare the timeout value with the given value and returns TRUE if the timeout has expired*/
bool Timeout_Check(unsigned long timeout)
{
  unsigned long cur_millis = 0;

  cur_millis = millis();

  /*check for overflow situation*/
  if(cur_millis < prev_millis)
  {
    return(false);  /*ignore the situation and hope for the best*/
  }

  /*normal situation, we can safely check for timeout overflow*/
  if((cur_millis - prev_millis) > timeout)
  {
    return(true);
  }
  else
  {
    return(false);
  }

}
