/*******************************************************************************

 Bare Conductive On Board Midi Piano (or other instrument... or drum kit!)
 -------------------------------------------------------------------------
 
 Midi_Piano.ino - basic sketch that defines the Touch Board as a particular
 instrument when placed in onboard MIDI mode. 

 To do this, connect the two solder bridges with "MIDI" and "MIDI ON" printed
 next the the solder pads on the Touch Board. You can do this with solder or
 using small blobs of Electric Paint.

 By default, this is a 12-key keyboard with just the white notes (a diatonic 
 scale). This can be changed to a full chromatic scale (black and white notes) 
 or a drum kit by modifying the indicated sections in loop() (and also 
 setupMIDI() if you want drums). You can also change this to be any instrument
 by modifying the Melodic Instruments section of setupMIDI().
 
 Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige. 
 Much thievery from Nathan Seidle in this particular sketch. Thanks Nate - 
 we owe you a cold beer!
 
 This work is licensed under a MIT license https://opensource.org/licenses/MIT
 
 Copyright (c) 2016, Bare Conductive
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*******************************************************************************/

// compiler error handling
#include "Compiler_Errors.h"

// serial rate
#define baudRate 57600

// include the relevant libraries
#include <MPR121.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(12, 10); // Soft TX on 10, we don't use RX in this code

// Touch Board Setup variables
#define firstPin 0
#define lastPin 11

// VS1053 setup
byte note = 0; // The MIDI note value to be played
byte resetMIDI = 8; // Tied to VS1053 Reset line
byte ledPin = 13; // MIDI traffic inidicator
int  instrument = 0;

// key definitions
const byte whiteNotes[] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81};
const byte allNotes[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};

// if you're using this with drum sounds instead of notes, you can choose each 
// individual drum sound from the bank shown at the bottom of this code
// where it says "PERCUSSION INSTRUMENTS (GM1 + GM2)"
const byte drumNotes[] = {60, 36, 41, 44, 56, 54, 58, 36, 72, 79, 76, 58};

void setup(){
  Serial.begin(baudRate);
  
  // uncomment the line below if you want to see Serial data from the start
  // while (!Serial);
  
  // Setup soft serial for MIDI control
  mySerial.begin(31250);
   
  // 0x5C is the MPR121 I2C address on the Bare Touch Board
  if(!MPR121.begin(0x5C)){ 
    Serial.println("error setting up MPR121");  
    switch(MPR121.getError()){
      case NO_ERROR:
        Serial.println("no error");
        break;  
      case ADDRESS_UNKNOWN:
        Serial.println("incorrect address");
        break;
      case READBACK_FAIL:
        Serial.println("readback failure");
        break;
      case OVERCURRENT_FLAG:
        Serial.println("overcurrent on REXT pin");
        break;      
      case OUT_OF_RANGE:
        Serial.println("electrode out of range");
        break;
      case NOT_INITED:
        Serial.println("not initialised");
        break;
      default:
        Serial.println("unknown error");
        break;      
    }
    while(1);
  }
  
  // pin 4 is the MPR121 interrupt on the Bare Touch Board
  MPR121.setInterruptPin(4);
  // initial data update
  MPR121.updateTouchData();

  // Reset the VS1053
  pinMode(resetMIDI, OUTPUT);
  digitalWrite(resetMIDI, LOW);
  delay(100);
  digitalWrite(resetMIDI, HIGH);
  delay(100);
  
  // initialise MIDI
  setupMidi();
  
  
}

void loop(){
   if(MPR121.touchStatusChanged()){
     
     MPR121.updateAll();
     
     // code below sets up the board essentially like a piano, with an octave
     // mapped to the 12 electrodes, each a semitone up from the previous
     for(int i=firstPin; i<=lastPin; i++){
       
       // you can choose how to map the notes here - try replacing "whiteNotes" with
       // "allNotes" or "drumNotes"
       note = whiteNotes[lastPin-i];
       if(MPR121.isNewTouch(i)){
         //Note on channel 1 (0x90), some note value (note), 75% velocity (0x60):
         noteOn(0, note, 0x60);
         Serial.print("Note ");
         Serial.print(note);
         Serial.println(" on");
       } else if(MPR121.isNewRelease(i)) {   
         // Turn off the note with a given off/release velocity
         noteOff(0, note, 0x60);  
         Serial.print("Note ");
         Serial.print(note);
         Serial.println(" off");       
       }
     }
   }
}

// functions below are little helpers based on using the SoftwareSerial
// as a MIDI stream input to the VS1053 - all based on stuff from Nathan Seidle

// Send a MIDI note-on message.  Like pressing a piano key.
// channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

// Send a MIDI note-off message.  Like releasing a piano key.
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

// Sends a generic MIDI message. Doesn't check to see that cmd is greater than 127, 
// or that data values are less than 127.
void talkMIDI(byte cmd, byte data1, byte data2) {
  digitalWrite(ledPin, HIGH);
  mySerial.write(cmd);
  mySerial.write(data1);

  // Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  // (sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);

  digitalWrite(ledPin, LOW);
}

// SETTING UP THE INSTRUMENT:
// The below function "setupMidi()" is where the instrument bank is defined. Use the VS1053 instrument library
// below to aid you in selecting your desire instrument from within the respective instrument bank


void setupMidi(){
  
  // Volume - don't comment out this code!
  talkMIDI(0xB0, 0x07, 127); //0xB0 is channel message, set channel volume to max (127)
  
  // ---------------------------------------------------------------------------------------------------------
  // Melodic Instruments GM1 
  // ---------------------------------------------------------------------------------------------------------
  // To Play "Electric Piano" (4):
  talkMIDI(0xB0, 0, 0x00); // Default bank GM1  
  // We change the instrument by changing the middle number in the brackets 
  // talkMIDI(0xC0, number, 0); "number" can be any number from the melodic table below
  talkMIDI(0xC0, 4, 0); // Set instrument number. 0xC0 is a 1 data byte command(55,0) 
  // ---------------------------------------------------------------------------------------------------------
  // Percussion Instruments (Drums, GM1 + GM2) 
  // ---------------------------------------------------------------------------------------------------------  
  // uncomment the two lines of code below to use - you will also need to comment out the two "talkMIDI" lines 
  // of code in the Melodic Instruments section above 
  // talkMIDI(0xB0, 0, 0x78); // Bank select: drums
  // talkMIDI(0xC0, 0, 0); // Set a dummy instrument number
  // ---------------------------------------------------------------------------------------------------------
  
}

/* MIDI INSTRUMENT LIBRARY: 

MELODIC INSTRUMENTS (GM1) 
When using the Melodic bank (0x79 - same as default), open chooses an instrument and the octave to map 
To use these instruments below change "number" in talkMIDI(0xC0, number, 0) in setupMidi()


0   Acoustic Grand Piano       32  Acoustic Bass             64  Soprano Sax           96   Rain (FX 1)
1   Bright Acoustic Piano      33  Electric Bass (finger)    65  Alto Sax              97   Sound Track (FX 2)
2   Electric Grand Piano       34  Electric Bass (pick)      66  Tenor Sax             98   Crystal (FX 3)
3   Honky-tonk Piano           35  Fretless Bass             67  Baritone Sax          99   Atmosphere (FX 4)
4   Electric Piano 1           36  Slap Bass 1               68  Oboe                  100  Brigthness (FX 5)
5   Electric Piano 2           37  Slap Bass 2               69  English Horn          101  Goblins (FX 6)
6   Harpsichord                38  Synth Bass 1              70  Bassoon               102  Echoes (FX 7)
7   Clavi                      39  Synth Bass 2              71  Clarinet              103  Sci-fi (FX 8) 
8   Celesta                    40  Violin                    72  Piccolo               104  Sitar
9   Glockenspiel               41  Viola                     73  Flute                 105  Banjo
10  Music Box                  42  Cello                     74  Recorder              106  Shamisen
11  Vibraphone                 43  Contrabass                75  Pan Flute             107  Koto
12  Marimba                    44  Tremolo Strings           76  Blown Bottle          108  Kalimba
13  Xylophone                  45  Pizzicato Strings         77  Shakuhachi            109  Bag Pipe
14  Tubular Bells              46  Orchestral Harp           78  Whistle               110  Fiddle
15  Dulcimer                   47  Trimpani                  79  Ocarina               111  Shanai
16  Drawbar Organ              48  String Ensembles 1        80  Square Lead (Lead 1)  112  Tinkle Bell
17  Percussive Organ           49  String Ensembles 2        81  Saw Lead (Lead)       113  Agogo
18  Rock Organ                 50  Synth Strings 1           82  Calliope (Lead 3)     114  Pitched Percussion
19  Church Organ               51  Synth Strings 2           83  Chiff Lead (Lead 4)   115  Woodblock
20  Reed Organ                 52  Choir Aahs                84  Charang Lead (Lead 5) 116  Taiko
21  Accordion                  53  Voice oohs                85  Voice Lead (Lead)     117  Melodic Tom
22  Harmonica                  54  Synth Voice               86  Fifths Lead (Lead 7)  118  Synth Drum
23  Tango Accordion            55  Orchestra Hit             87  Bass + Lead (Lead 8)  119  Reverse Cymbal
24  Acoustic Guitar (nylon)    56  Trumpet                   88  New Age (Pad 1)       120  Guitar Fret Noise
25  Acoutstic Guitar (steel)   57  Trombone                  89  Warm Pad (Pad 2)      121  Breath Noise
26  Electric Guitar (jazz)     58  Tuba                      90  Polysynth (Pad 3)     122  Seashore 
27  Electric Guitar (clean)    59  Muted Trumpet             91  Choir (Pad 4)         123  Bird Tweet
28  Electric Guitar (muted)    60  French Horn               92  Bowed (Pad 5)         124  Telephone Ring
29  Overdriven Guitar          61  Brass Section             93  Metallic (Pad 6)      125  Helicopter
30  Distortion Guitar          62  Synth Brass 1             94  Halo (Pad 7)          126  Applause
31  Guitar Harmonics           63  Synth Brass 2             95  Sweep (Pad 8)         127  Gunshot  

PERCUSSION INSTRUMENTS (GM1 + GM2)

When in the drum bank (0x78), there are not different instruments, only different notes.
To play the different sounds, select an instrument # like 5, then play notes 27 to 87.
 
27  High Q                     43  High Floor Tom            59  Ride Cymbal 2         75  Claves 
28  Slap                       44  Pedal Hi-hat [EXC 1]      60  High Bongo            76  Hi Wood Block
29  Scratch Push [EXC 7]       45  Low Tom                   61  Low Bongo             77  Low Wood Block
30  Srcatch Pull [EXC 7]       46  Open Hi-hat [EXC 1]       62  Mute Hi Conga         78  Mute Cuica [EXC 4] 
31  Sticks                     47  Low-Mid Tom               63  Open Hi Conga         79  Open Cuica [EXC 4]
32  Square Click               48  High Mid Tom              64  Low Conga             80  Mute Triangle [EXC 5]
33  Metronome Click            49  Crash Cymbal 1            65  High Timbale          81  Open Triangle [EXC 5]
34  Metronome Bell             50  High Tom                  66  Low Timbale           82  Shaker
35  Acoustic Bass Drum         51  Ride Cymbal 1             67  High Agogo            83 Jingle bell
36  Bass Drum 1                52  Chinese Cymbal            68  Low Agogo             84  Bell tree
37  Side Stick                 53  Ride Bell                 69  Casbasa               85  Castanets
38  Acoustic Snare             54  Tambourine                70  Maracas               86  Mute Surdo [EXC 6] 
39  Hand Clap                  55  Splash Cymbal             71  Short Whistle [EXC 2] 87  Open Surdo [EXC 6]
40  Electric Snare             56  Cow bell                  72  Long Whistle [EXC 2]  
41  Low Floor Tom              57  Crash Cymbal 2            73  Short Guiro [EXC 3]
42  Closed Hi-hat [EXC 1]      58  Vibra-slap                74  Long Guiro [EXC 3]

*/


