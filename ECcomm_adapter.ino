/* ECcomm_adapter - Microcontroller interface for General Motors
            Entertainment & Comfort serial bus communication.

    Copyright (C) 2016 Stuart V. Schmitt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.



    This code assumes a 16 MHz AVR with the E&C bus connected through
    inverting transistors to digital pins 3 and 5. Direct port registers
    are used because the overhead associated with Arduino functions
    digitalRead and digitalWrite result in too many lost or damaged
    E&C packets. For more information about the bus or the electrical
    interface, please look at information at
        http://stuartschmitt.com/e_and_c_bus/                      */

#include "packet.h"
#include "statuses.h"

#define MSG_IS(a, b, c) (msg.Priority() == a && msg.Address() == b && msg.Byte(0) == c)
#define MSG_IS_EXT(a, b, c, d) (msg.Priority() == a && msg.Address() == b && msg.Byte(0) == c && msg.Byte(1) == d)
#define PIN_RX 22
#define PIN_TX 23

byte mode = 0;  // 0=ASCII, 1=binary; add 2 if an E&C packet is
// being sent from the computer (ESC + 4 bytes)
unsigned long timer = 0;  // Timestamp of last E&C packet received/sent

byte bus_state = 0;
PlayerStatus playerStatus = UNSELECTED;

// 4 is acc power
// 2 is OnStar
// 1 is audio system
// Use macros from statuses.h
volatile byte power_status = 0;
volatile byte sent_disc_data = false;
volatile byte module_present_number = 0;

RTC_DATA_ATTR bool battery_disconnected = true;

void setup() {
  pinMode(PIN_TX, OUTPUT);
  Serial.begin(115200);  // No need for faster serial; this is 9.6x E&C
}


void loop() {
  bus_state = digitalRead(PIN_RX);
  if (bus_state) {  // If the bus is active, start a read.
    Packet msg(PIN_RX, PIN_TX);
    msg.Read();
    if (msg.Sent()) {  // Here, Sent() filters out some false triggers
      //Serial logging
      /*
      if (mode % 2) {
        WriteBinary(msg.Unique(), millis() - timer, msg.Incomplete());
      } else {
        if (msg.Priority() == 2) {
          Serial.print("     ");
        } else if (msg.Priority() == 3) {
          Serial.print("          ");
        }
        WriteText(msg.Unique(), millis() - timer, msg.Incomplete());
      }
      */
      Serial.print("↓");
      WriteText(msg.Unique(), millis() - timer, msg.Incomplete());
      // return;

      //
      // Communication
      //
      Packet send(PIN_RX, PIN_TX);

      // Power status
      if (MSG_IS(1, 40, 63)) {
        power_status = msg.Byte(1);
        if (!ACC_POWER(power_status)) {
          // TODO: Deep sleep
        }

        goto SkipIfs;
      }

      // Is module present?
      if (MSG_IS_EXT(3, 30, 153, 0)) {
        switch (module_present_number) {
          case 0:
            // Mechanism status
            // HIJ (magazine is present / playback not possible / always 0?)
            send.Send(1, 26, 9, 192, 1);
            if (battery_disconnected) {
              // Disc read status
              // DI (done seeking / battery was disconnected)
              send.Send(1, 26, 1, 132);
              delay(50);
              // Disc read status
              // D (done seeking)
              send.Send(1, 26, 1, 4);
              battery_disconnected = false;
            } else {
              // Disc read status
              // H (disc data does not need updating)
              send.Send(1, 26, 1, 64);
            }
            break;
          default:
            // Mechanism status
            // AHIJ (ready / magazine is present / playback not possible / always 0?)
            send.Send(1, 26, 137, 192, 1);
            // Disc read status
            // H (disc data does not need updating)
            send.Send(1, 26, 1, 64);
            break;
        }

        module_present_number++;

        goto SkipIfs;
      }

      // Upload disc data
      if (MSG_IS_EXT(3, 30, 153, 63)) {
        // Discs present
        // D
        send.Send(1, 26, 29, 4);
        delay(30);
        // Disc number
        // 1 (translated from 41/7 + A or 0b1001010_1)
        send.Send(2, 26, 169);
        delay(30);
        // Begin disc data
        send.Send(2, 26, 209);
        delay(30);
        // Disc track count
        // 10
        send.Send(2, 26, 89, 8);
        delay(30);
        // Disc time minutes
        // 42
        send.Send(2, 26, 97, 33);
        delay(30);
        // Disc time seconds
        // 57
        send.Send(2, 26, 233, 43);
        delay(30);
        // Disc time frames
        // 62
        send.Send(2, 26, 25, 49);
        delay(30);
        // Mechanism status
        // AFHJ (ready / disc data does not need updating / magazine is present / always 0?)
        send.Send(1, 26, 137, 80, 1);
        delay(30);
        // Disc read status
        // H (disc data does not need updating)
        send.Send(1, 26, 1, 64);
        delay(30);

        goto SkipIfs;
      }

      // Load disc
      if (MSG_IS(3, 30, 217)) {
        /*
        TODO: Maybe send correct disc number; honestly 1 might be fine
        unsigned char disc_number_lsd = (0b1110 & msg.Byte(1));
        unsigned char disc_number_gsd = ((0b11100000 & msg.Byte(1)) >> 5) + (msg.Byte(2) << 3);
        unsigned char disc_number = disc_number_lsd + (10 * disc_number_gsd);
        */

        // Disc read status
        // D (done seeking)
        // FIXME: might have to send an AD then a D
        send.Send(1, 26, 1, 4);
        delay(30);
        // Disc number
        // 1
        send.Send(2, 26, 169);

        goto SkipIfs;
      }

      // Stop playback
      if (MSG_IS(3, 30, 145)) {
        // Disc read status
        // H (disc data does not need updating)
        send.Send(1, 26, 1, 64);

        // TODO: pause

        goto SkipIfs;
      }

      // CD changer selected
      if (MSG_IS(3, 44, 99)) {
        playerStatus = SELECTED;
        // Disc read status
        // AD (laser on / done seeking)
        send.Send(1, 26, 129, 4);
        // Disc read status
        // AB (laser on / playing)
        send.Send(1, 26, 129, 1);
        // Playback track
        // 1
        send.Send(2, 26, 177);

        // TODO: Play and begin sending heartbeats

        goto SkipIfs;
      }

      if (MSG_IS(3, 30, 145)) {
        playerStatus = UNSELECTED;
        // TODO: Pause
        // Disc read status
        // AH (laser on / disc data does not need updating)
        send.Send(1, 26, 129, 64);
        // Disc read status
        // H (disc data does not need updating)
        send.Send(1, 26, 1, 64);
      }

      // TODO: Implement seeking (maybe)

      // Didn't recognize the packet; just print it
      // WriteText(msg.Unique(), millis() - timer, msg.Incomplete());
      // WriteBinary(msg.Unique(), millis() - timer, msg.Incomplete());

SkipIfs:
      timer = millis();
    }
  }
}
