# TeleJukebox
A project based on the concepts of the Wonderfoon and Arduinofoon

This project came to my attention through the news item from:  
https://nos.nl/video/2299835-de-wonderfoon-voor-dementerende-ouderen.html  
  
That showed how eldery people loved the sight of an old-skool rotary telephone, playing familiar songs of the past.
This project seemed to be focussed on people who have Alzheimer. No Because I'm a technician, my wife work works with elderly people, asked me if I could build such a phone. She already did a quick google that brought me to this website:  
http://www.wonderfoon.nl  
It showed how you could make such a phone, but although I love the project, it really hurt my technical heart to see how it required these perfectly fine working phones to be dismanteld, ruined, destroyed. This shouldn't be required to make this wonder of 1965 technology (a simple phone) perform this simple new task. Also the complexity of the build (and the work required to assemble it) slightly shocked me. Now couldn't this be more simple, eventually more cheaply, just a simple PCB in an small case where you plug the phone in. A simpler design could result in more of these devices being made and therefore more joy among the eldery could be achieved. Also... many innocent phones could be saved from being permanently being crippled beyond repair.
  
  
The project called "Wonderfoon" was initially created by Leo Willems but gradually this project evolved.
https://www.gelderlander.nl/nijmegen/wonderfoon-laat-dementerende-ouderen-luisteren-naar-liedjes-van-vroeger~adb0f781/?referrer=https://www.google.com/  
There is one iteration called the "Arduinofoon", a much more simplified version of the Wonderfoon, but it still requires the phone to be heavily modified.  
https://www.repaircafehengelo.nl/arduinofoon/  

Now my version of this musical phone, works with the very small but versatile Arduino Pro Micro, uses a simple $1 MP3 player, optocoupler some capacitors and resistors to tie it all together.  
The TeleJukebox does not require you phone to be modifed beyond repair. All that is required is the shorting of the microphone wires. Which can be done with a piece of wire in the terminal block on the bottom of the phone OR if you are really lazy, using a piece of aluminum foil in the microphone section of the handpiece.
  
The completed project works very simple:
- Pick up the phone, you hear the "dial tone" MP3,
  Dial a number, you hear the MP3 0..9 playing (depending on the dialed number),
  When the song is over, you hear the "disconnected" MP3,
  You can dial another number and hear another song or hang up the phone.
- Some phones also have a button on the front. When you press this button, the phone randomly playes one of the songe 0..9 and when that song is over, it automatically plays a new song, randomly. This will allow the person on the phone to continuously listen to the music. This may be of help to those who can't figure out how it works but love to hear the songs over and over again.
- You can configure the music folder from which the MP3s are played, you can use up to 10 different folders  
- You can configure the volume of the device  
- Configurations are done through the phone itself (dial 738 OR hold the button while picking up the handpiece), you hear a voice that guides you through a menu and all you need to do is dial in your settings value.

You can power this design through a USB port cable. This way the device can be fed using a simple mobilephone charger OR be connected to the PC. When connected to a PC, the TeleJukebox identifies itself like a USB-stick and you can easily modify the file without removing the card from the system. Meaning that you do no even need to open the case in order to change the collection of MP3 files. Keep in mind that when the device is in USB-stick mode, it cannot playback MP3 files meaning that in this mode it will not act like a TeleJukebox.

# Read the manual
If you want to know in detail how the TeleJukebox works (from a userpoint perspective) then please read the user manual, which is in the folder named "manual". This manual also describes how the SD_card should be configured.

# Pro micro driver issues
For some reason some Pro Micro's require drivers while others are recognized straight out of the box.  
For those who have a Sparkfun Pro Micro and are in need of drivers the following information can be helpfull:  

Add in the Arduino IDE instellingen the following line to the list of boards manager URL’s:  
https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json  

Then you can select in the boards manager ‘Sparkfun AVR boards’ and install it. Choose 'Sparkfun Pro Micro’ and make sure to select the 5V version.  

The USB driver consists of a .inf and a .cat file, which can be downloaded from github:  
https://github.com/sparkfun/Arduino_Boards/raw/master/sparkfun/avr/signed_driver/sparkfun.inf  
https://github.com/sparkfun/Arduino_Boards/raw/master/sparkfun/avr/signed_driver/sparkfun.cat  

# Phone issues
Not all phones are equal, but it can be of help to know what phone and wiring scheme inside the phone I used.  
So I made a photo of my phone and it's wiring, you can find it in the folder named "photo".