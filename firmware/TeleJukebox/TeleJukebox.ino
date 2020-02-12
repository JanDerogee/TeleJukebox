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
#define dbg_jmpr          2     /*simple jumper, when placed the firmware does something safe, so the bootloader can be operated without problems*/
#define dbg_LED           6     /*simpe LED useful for all sorts of problems*/
#define mp3_player_busy   7     /*the pin of the MP3 player indicatingthat the device is busy*/
#define ph_hookpulse_a    A8    /*this signal is processed as being analog for better noise suppression*/
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
unsigned char phone_state = STATE_HOOK_DOWN;    /*the statemachine of the phones behaviour*/
unsigned char mp3_state = MP3_IDLE;             /*the statemachine of the phones behaviour*/
//unsigned char lp = 0;

/*-----------------------------------------------------------------------*/

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
        while(digitalRead(ph_button) == 0)      /*keep looping until released, produce a tone while pressing the button*/
        {
          delay(1);
        }
        
        Serial.println("button detect");        
        phone_state = STATE_WAIT_FOR_INPUT;
        ret_val = PHONE_BUTTON;
        break;      
      }
      
      if(CheckHookPulseSignal() == HIGH)  /*check for pulse activity*/
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
      if(CheckHookPulseSignal() == LOW)       /*check if hook is picked up*/
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
  if(str == "002")  /*this is the old Dutch number for the current time*/
  {
    Serial.print("002 = old Dutch number for the current timer");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,0002);   /*play from folder "02" the file "0002.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "003")  /*this is the old Dutch number for the weather*/
  {
    Serial.print("003 = old Dutch number for the weather");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,0003);   /*play from folder "02" the file "0003.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "008")  /*this is the old Dutch number for telephone number information*/
  {
    Serial.print("008 = old dutch number for telephone number related information");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,8);      /*play from folder "02" the file "0008.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "1945")  /*this is the year of birth of Deborah Harry (a.k.a. Blondie), she mad a sone about a telephone*/
  {
    Serial.print("1945 = year of birth of Deborah Harry (a.k.a. Blondie)");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1945);   /*play from folder "02" the file "1947.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/        
    return(true); /*easter egg was detected and handled*/    
  }
  else
  if(str == "1947")  /*this is the year of birth of one of the greatest dutch commedians (Andre van Duin), he made a funny song about a telephone*/
  {
    Serial.print("1947 = year of birth of Adrianus Marinus Kyvon (a.k.a. Andre van Duin)");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1947);   /*play from folder "02" the file "1947.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/        
    return(true); /*easter egg was detected and handled*/    
  }
  else
  if(str == "1950")  /*Stevie Wonder was born in 1950, he made a song about a telephone call*/
  {
    Serial.print("1950 = year of birth of Stevie Wonder");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1950);   /*play from folder "02" the file "1950.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }  
  else
  if(str == "1969")  /*1969 was the year of the moonlanding, this is how dutch television reported about that miraculous event*/
  {
    Serial.print("1969 = the moonlanding as reported by the dutch televison");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1969);   /*play from folder "02" the file "1969.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "1976")  /*Lady Gaga was born in 1986, years later she made a sone called Telephone*/
  {
    Serial.print("1976 = cool french song about a TELEPHONE");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1976);   /*play from folder "02" the file "1976.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  
  else
  if(str == "1986")  /*Lady Gaga was born in 1986, years later she made a sone called Telephone*/
  {
    Serial.print("1986 = year of birth of Lady Gaga");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,1986);   /*play from folder "02" the file "1986.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
    return(true); /*easter egg was detected and handled*/
  }
  else
  if(str == "2006")  /*Soundtrack of a dutch "Hi" (which is a telecom provider) commercial from 2006*/
  {
    Serial.print("2006 = Hi5 Band - Met Z'n Allen Op Een Lijn!");
    myDFPlayer.stop();                    /*song has ended, play the disconnected sound*/
    delay(200);                           /*allow some time to execute command*/
    myDFPlayer.playLargeFolder(2,2006);   /*play from folder "02" the file "2006.MP3" containing the easter egge related sound*/         
    mp3_state = MP3_PLAYING_ALLOW_CHANGE; /*allow the selection of a different MP3 during the playback of this MP3*/    
    delay(200);                           /*some time to get the MPO3 started and the busy signal to rise*/    
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
