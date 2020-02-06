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

Now my version of this musical phone, works with the very small but versatile Arduino Pro Micro, uses a simple $1 MP3 player an optocoupler some resistors and a two small capacitors but most importantly, it DOES NOT require any modification to the phone itself. As all electronics can be placed inside a small box to which the telephone connects through it's existing PTT phone connector. I did not take the effort of making this design into a PCB that's easy to assemble. Because I needed only one phone, I decided it was quicker to solder all the parts onto a piece of perfboard and stick it in an improvised case. Now for those who do want to go "into production" feel free to make a PCB, feel free to 3D print a nice case or modify an existing wall mounted case to hold it all. The circuit isn't big so that should not be a problem. The volume of the phone is determined by the resistor in series with the MP3 player. It is loud enough though same may say it should be louder. Though I doubt that that would be a good idea. One reason why the resistor values are choosen this way is that the MP3 player slightly interferes with the pulse and hook detection. All signals are on the same wire so they will interefer. With these values it all works fine. But if you want to increase the volume by lowering the value of the resistor in series with the MP3 player, then you might get in trouble with pulse detection. So be aware of this if you start tinkering with values.  
  
The completed project works very simple:
- Pick up the phone, you hear the "dial tone" MP3
- Dial a number, you hear the MP3 0..9 playing (depending on the dialed number)
- When the song is over, you hear the "disconnected" MP3
- You can dial another number and hear another song or hang up the phone
- Some phones also have a button on the front. When you press this button, the phone randomly playes one of the songe 0..9 and when that song is over, it automatically plays a new song, randomly. This will allow the person on the phone to continuously listen to the music. This may be of help to those who can't figure out how it works but love to hear the songs over and over again.

And that's it. Now the code holds some routines capable of handling easter eggs, but these are commented out. Mainly because it would be confusing to the elderly people when something unexpected happens and they might be under the impression that something is wrong or broken.

The "dialtone" is just a simple low frequency "beeeeeep", you might want to replcae this with a file that says something.
For instance saying "this is the wonderfoon, dial a number to play some music... this is the wonderphone etc."
Feel free to play with the files and the content. The names however are very important, use the naming conventions as shown below, the player depends on seeing numbers. Prepare the files on your PC and upload all the files at once, this way the files 0..9 are stored on the card in the same order as the filename dictates.

You can power this design through a USB port cable. This way the device can be fed using a simple mobilephone charger OR be connected to the PC, in the latter it will be recognized as a USB-stick and files can easily be modified without changing the card. Meaning that you do no even need to open the case in order to reconfigure it.


# In order for the device to work the SD-card in the MP3 player needs to hold the following files and folders
The SD-card must contain the following files and folders:

#### MP3  
Files in this folder "MP3" are coupled to the numbers on the dial  
The files must have the following name convention (max number of files is 10)  
  
MP3/0000-name of song.mp3 = this is the song that will be played when the user dials a 0  
MP3/0001-name of song.mp3	= this is the song that will be played when the user dials a 1  
MP3/0002-name of song.mp3	= this is the song that will be played when the user dials a 2  
MP3/0003-name of song.mp3	= this is the song that will be played when the user dials a 3  
MP3/0004-name of song.mp3	= this is the song that will be played when the user dials a 4  
MP3/0005-name of song.mp3	= this is the song that will be played when the user dials a 5  
MP3/0006-name of song.mp3	= this is the song that will be played when the user dials a 6  
MP3/0007-name of song.mp3	= this is the song that will be played when the user dials a 7  
MP3/0008-name of song.mp3	= this is the song that will be played when the user dials a 8  
MP3/0009-name of song.mp3	= this is the song that will be played when the user dials a 9  
  
#### 01
Files in this folder contains files for producing the service tones

01/0001-dialtone.MP3 = dialtone file  
01/0002-disconnected.MP3 = disconnected file  
01/0003-number_out_of_service.MP3	= number has been disconnected or is no longer in service  


# Pro micro driver issues
For some reason some Pro Micro's require drivers while others are recognized straight out of the box.
For those who have a Sparkfun Pro Micro and are in need of drivers the following information can be helpfull:
Installatie instructies:

Add in the Arduino IDE instellingen the following line to the list of boards manager URL’s:
https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json

Then you can select in the boards manager ‘Sparkfun AVR boards’ and install it. Choose 'Sparkfun Pro Micro’ and make sure to select the 5V version.

The USB driver consists of a .inf and a .cat file, which can be downloaded from github:
https://github.com/sparkfun/Arduino_Boards/raw/master/sparkfun/avr/signed_driver/sparkfun.inf
https://github.com/sparkfun/Arduino_Boards/raw/master/sparkfun/avr/signed_driver/sparkfun.cat

# Phone issues
Not all phones are equal, although they can be, some are wired differently.
The phone I used had a very simple wiring and I made a photo of it, which can be found in photo folder.