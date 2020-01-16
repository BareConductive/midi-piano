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

// touch includes
#include <MPR121.h>
#include <MPR121_Datastream.h>
#include <Wire.h>

// MIDI includes
#include <SoftwareSerial.h>

// touch constants
const uint32_t BAUD_RATE = 115200;
const uint8_t MPR121_ADDR = 0x5C;
const uint8_t MPR121_INT = 4;

// MPR121 datastream behaviour constants
const bool MPR121_DATASTREAM_ENABLE = false;

// MIDI variables
uint8_t note = 0;  // MIDI note value to be played

// MIDI constants
SoftwareSerial mySerial(12, 10);  // soft TX on 10, we don't use RX in this code
const uint8_t RESET_MIDI = 8;  // tied to VS1053 Reset line

// MIDI behaviour constants
const uint8_t MELODIC_NOTES[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};  // C4 octave notes for melodic instruments
const uint8_t PERCUSSION_NOTES[] = {49, 35, 57, 36, 41, 51, 43, 28, 27, 83, 76, 58};  // percussion sounds
                                                                                      // change numbers to play different percussion sounds
                                                                                      // a full overview of instruments can be found here:
                                                                                      // http://bareconductive.com/assets/resources/MIDI%20Instruments.pdf
const bool MELODIC_INSTRUMENT = true;  // set to "false" to play percussion sounds
const uint8_t INSTRUMENT = 0;  // change instrument number according to MIDI instrument library
                               // a full overview of instruments can be found here:
                               // http://bareconductive.com/assets/resources/MIDI%20Instruments.pdf


void setup() {
  Serial.begin(BAUD_RATE);

  mySerial.begin(31250);  // setup soft serial for MIDI control

  if (!MPR121.begin(MPR121_ADDR)) {
    Serial.println("error setting up MPR121");
    switch (MPR121.getError()) {
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
    while (1);
  }

  MPR121.setInterruptPin(MPR121_INT);

  if (MPR121_DATASTREAM_ENABLE) {
    MPR121.restoreSavedThresholds();
    MPR121_Datastream.begin(&Serial);
  } else {
    MPR121.setTouchThreshold(40);
    MPR121.setReleaseThreshold(20);
  }

  MPR121.setFFI(FFI_10);
  MPR121.setSFI(SFI_10);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // switch on user LED while auto calibrating electrodes

  MPR121.setGlobalCDT(CDT_4US);  // reasonable for larger capacitances at the end of long cables when using Interactive Wall Kit
  MPR121.autoSetElectrodeCDC();  // autoset all electrode settings

  digitalWrite(LED_BUILTIN, LOW);

  // reset MIDI player
  pinMode(RESET_MIDI, OUTPUT);
  digitalWrite(RESET_MIDI, LOW);
  delay(100);
  digitalWrite(RESET_MIDI, HIGH);
  delay(100);

  // initialise MIDI
  setupMidi();
}

void loop() {
  MPR121.updateAll();

  for (int i=0; i< 12; i++) {
    if (MELODIC_INSTRUMENT) {
      note = MELODIC_NOTES[11-i];  // map the 12 electrodes to an octave, each a semitone up from the previous, starting from E11
    } else {
      note = PERCUSSION_NOTES[11-i];  // map the 12 electrodes to various drum sounds, starting from E11
    }
    
    if (MPR121.isNewTouch(i)) {
      // note on channel 1 (0x90), some note value (note), 75% velocity (0x60):
      noteOn(0, note, 0x60);

      if (!MPR121_DATASTREAM_ENABLE) {
        Serial.print("Note ");
        Serial.print(note);
        Serial.println(" on");
      }
     } else if (MPR121.isNewRelease(i)) {
      // turn off the note with a given off/release velocity
      noteOff(0, note, 0x60);

      if (!MPR121_DATASTREAM_ENABLE) {
        Serial.print("Note ");
        Serial.print(note);
        Serial.println(" off");
      }
     }
   }

   if (MPR121_DATASTREAM_ENABLE) {
    MPR121_Datastream.update();
  }
}

void noteOn(byte channel, byte note, byte attack_velocity) {
  // send a MIDI note-on message, ike pressing a piano key, channel ranges from 0-15
  talkMIDI((0x90 | channel), note, attack_velocity);
}

void noteOff(byte channel, byte note, byte release_velocity) {
  // send a MIDI note-off message, ike releasing a piano key
  talkMIDI((0x80 | channel), note, release_velocity);
}

void talkMIDI(byte cmd, byte data1, byte data2) {
  // sends generic MIDI message
  digitalWrite(LED_BUILTIN, HIGH);
  mySerial.write(cmd);
  mySerial.write(data1);

  if ((cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);

  digitalWrite(LED_BUILTIN, LOW);
}

void setupMidi() {
  talkMIDI(0xB0, 0x07, 127);  // 0xB0 is channel message, set channel volume to max (127)

  if (MELODIC_INSTRUMENT) {
    talkMIDI(0xB0, 0, 0x00);  // default bank GM1
  } else {
    talkMIDI(0xB0, 0, 0x78);  // bank GM1 + GM2
  }
  
  talkMIDI(0xC0, INSTRUMENT, 0);  // set instrument number, 0xC0 is a 1 data byte command(55,0)
}
