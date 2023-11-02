/* packet.h - Support library for Arduino sketches for communication
            with the General Motors Entertainment & Comfort serial bus. 

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
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef packet_h
#define packet_h
#include "Arduino.h"

const unsigned zero = 111;         // microseconds
const unsigned one = 667;          // microseconds
const unsigned bit_length = 1000;  // microseconds
const unsigned maxloops = 10000;    // clock cycles. This assumes 80 MHz operation.
const unsigned waitloops = 50000;  // clock cycles. 15000 scan tool, 10000 in-car module

void WriteBinary(unsigned long packet, unsigned long time_gap, byte repaired);
void WriteText(unsigned long packet, unsigned long time_gap, byte repaired);

class Packet {
  private:
    byte header;         // two bits for priority, six bits for address
    unsigned long data;  // 24 bits
    boolean parity;

    byte pin_in;         // set by constructor
    byte pin_out;        // set by constructor
    byte counter;        // index of bit read from bus; init to 0 in constructor
    byte num_bits;       // number of data bits; even and >= 2
    boolean was_sent;    // flag if packet actually was on the bus (read or send)
    boolean incomplete;  // flag if packet was received incomplete

    void AddBit(boolean bit_val) {
      // Called by Read(); appends the bit to the packet being read from the bus.
      if (counter < 8)
        bitWrite(header, counter, bit_val);
      else
        bitWrite(data, counter-8, bit_val);
      counter++;
    }

    void ErrorCheck() {
      // Check to see if the packet was received correctly. If not, attempt to fix.
      // Called at the end of Read().
      boolean calc_parity = CalcParity();
      if (num_bits % 2 == 0) {
        if (calc_parity != parity) {
          data = (data << 2) + (header >> 6);
          header = (header << 2) + 1;
          num_bits += 2;
          incomplete = true;
        }
        else if (header == 0) {
          data = (data << 2) + (header >> 6);
          header = (header << 2) + 3;
          num_bits += 2;
          incomplete = true;
        }
      }
      else {
        data = (data << 1) + (header >> 7);
        header <<= 1;
        num_bits++;
        if (calc_parity != parity)
          header++;
        else
          ErrorCheck();
        incomplete = true;
      }
    }

    boolean CalcParity() {
      // Calculate the parity bit. Called by ErrorCheck() and Set().
      boolean val = bitRead(header, 0);
      for (byte i = 1; i < 8; i++)
        val ^= bitRead(header, i);
      for (byte i = 0; i < num_bits; i++)
        val ^= bitRead(data, i);
      return val;
    }

/*    byte StripByte(String &input) {
      // Return the number (0-255) at the beginning of a string
      byte val;
      if (input.indexOf(' ') > 0) {
        String bytestring = input.substring(0, input.indexOf(' '));
        val = bytestring.toInt();
        input = input.substring(input.indexOf(' ')+1);
      }
      else {
        val = input.toInt();
        input = "";
      }
      return val;
    }*/

    void SendBit(boolean val) {
      // Send a single bit on the E&C bus.
      // Serial.print(val);
      digitalWrite(pin_out, 1);
      if (val) {
        delayMicroseconds(one);
        digitalWrite(pin_out, 0);
        delayMicroseconds(bit_length - one);
      }
      else {
        delayMicroseconds(zero);
        digitalWrite(pin_out, 0);
        delayMicroseconds(bit_length - zero);
      }
    }

  public:
    Packet(byte reg_rx, byte reg_tx) {
      // Constructor
      header = 0;
      data = 0;
      parity = false;
      
      pin_in = reg_rx;
      pin_out = reg_tx;
      counter = 0;
      num_bits = 0;
      was_sent = false;
      incomplete = false;
    }

    void Read() {
      // Read a packet from the E&C bus and save it to the object.
      unsigned long loops = 0;
      boolean keep_reading = true;
      // Close out start bit
      while (digitalRead(pin_in) && (loops++ < maxloops));
      // If the start bit lasts forever, there is no bus!
      if (loops > maxloops) {
        return;
      }
      //
      while (keep_reading) {
        // Check if there are no more bits
        while (!(digitalRead(pin_in)) && keep_reading) {
          if (loops++ > maxloops)
            keep_reading = false;
        }
        if (keep_reading) {
          loops = 0;
          while (digitalRead(pin_in))
            loops++;
          if (loops < 2500)
            AddBit(false);
          else
            AddBit(true);
        }
      }
      num_bits = (counter > 8) ? (counter - 9) : 0;
      // Move the parity bit from the last one added data
      parity = bitRead(data, num_bits);
      bitWrite(data, num_bits, 0);
      // Check for receive errors
      ErrorCheck();
      // Set was_sent to indicate that the message came from a functioning bus
      was_sent = true;
    }

    void Send() {
      // Send the packet stored in the object
      if (num_bits && pin_out) {
        // Wait for the bus to clear
        unsigned loops = 0;
        unsigned tries = 0;
        while (loops < waitloops) {
          loops++;
          if (digitalRead(pin_in)) {
            loops = 0;
            tries++;
          }
          else
            tries = 0;
          if (tries > maxloops)  // There's no bus in this case
            return;
        }
        SendBit(true);           // Start bit
        for (byte i = 0; i < 8; i++)
          SendBit(bitRead(header, i));
        for (byte i = 0; i < num_bits; i++)
          SendBit(bitRead(data, i));
        SendBit(parity);         // Parity bit
        Serial.println();
        was_sent = true;
      }
    }

    void Send(byte priority, byte address, byte data1 = 0, byte data2 = 0, byte data3 = 0) {
      Serial.print("↑");
      // Shorthand overload to avoid having to call Set() and then Send().
      if (pin_out) {
        Set(priority, address, data1, data2, data3);
        WriteText(this->Unique(), 0, this->Incomplete());
        Send();
      }
    }

    boolean Sent() {
      if (was_sent)
        return true;
      else
        return false;
    }

    void Set(byte priority, byte address, byte data1, byte data2, byte data3) {
      // Set the packet: 2-bit priority, 6-bit address, three 8-bit data bytes
      header = (address << 2) + (priority & 3);
      data = ((unsigned long) data3 << 16) + ((unsigned long) data2 << 8) + data1;
      num_bits = 2;
      while (data >> num_bits)
        num_bits += 2;
      parity = CalcParity();
    }

    void Set(byte priority, byte address, unsigned long longdata) {
      // Set the packet 2-bit priority, 6-bit address, 24-bit data
      header = (address << 2) + (priority & 3);
      data = longdata & 16777215;
      num_bits = 2;
      while (data >> num_bits)
        num_bits += 2;
      parity = CalcParity();
    }

/*    void Set(String input) {
      // Set the packet using a formatted string
      byte priority = StripByte(input);
      byte address = StripByte(input);
      byte byte1 = StripByte(input);
      byte byte2 = StripByte(input);
      byte byte3 = StripByte(input);
      Set(priority, address, byte1, byte2, byte3);
    }*/

    void Set(unsigned long unique) {
      // Set the packet using a single 32-bit number
      Set(unique & 3, (unique & 252) >> 2, unique >> 8);
    }

    byte Priority() {
      return header & 3;
    }

    byte Address() {
      return header >> 2;
    }

    byte BitRange(byte first, byte last) {
      byte value = data >> first;
      for (byte i = (last-first+1); i < 8; i++)
        value &= ~(1 << i);
      return value;
    }

    byte Byte(byte i) {
      // Return a byte from the data: 0, 1, or 2.
      return (data >> (i*8)) & 255;
      // return BitRange(i*8, i*8 + 7);
    }

    unsigned long Unique() {
      // Return the unique 32-bit value that contains both header and data
      return header + (data << 8);
    }

    byte Incomplete() {
      return incomplete;
    }
};

void WriteBinary(unsigned long packet, unsigned long time_gap, byte repaired) {
  // Send an encoded 12-byte sequence to the computer that contains the time
  // since the last E&C packet in milliseconds, the flag for incomplete E&C
  // reception, and the E&C packet contents. The start byte is an ESC, and the
  // subsequent bytes are limited to ASCII 32-95 in order to keep the tty sane
  // regardless of the terminal program. The bytes are packed in a method
  // based on UUencode, which allows six usable bits per byte.
  //   bits 1-32:   time gap between E&C packets
  //   bit 33:      incomplete E&C reception flag
  //   bit 34:      zero
  //   bits 35-66:  the E&C packet
  Serial.write((byte)27);
  Serial.write(32 + (time_gap & 0x3F));
  Serial.write(32 + ((time_gap & 0xFC0) >> 6));
  Serial.write(32 + ((time_gap & 0x3F000) >> 12));
  Serial.write(32 + ((time_gap & 0xFC0000) >> 18));
  Serial.write(32 + ((time_gap & 0x3F000000) >> 24));
  Serial.write(32 + (((time_gap & 0xC0000000) >> 30) + ((repaired & 3) << 2) + ((packet & 3) << 4)));
  Serial.write(32 + ((packet & 0xFC) >> 2));
  Serial.write(32 + ((packet & 0x3F00) >> 8));
  Serial.write(32 + ((packet & 0xFC000) >> 14));
  Serial.write(32 + ((packet & 0x3F00000) >> 20));
  Serial.write(32 + ((packet & 0xFC000000) >> 26));
}


void WriteText(unsigned long packet, unsigned long time_gap, byte repaired) {
  // Simply print the E&C packet to the serial port. This is convenient when
  // using a simple terminal program to connect to the interface. First, print
  // the time since the last E&C packet in milliseconds. Then print a ":"
  // unless the packet was received incomplete; then print a "#". Then print
  // the E&C packet in PRIORITY-ADDRESS-BYTE1-[BYTE2]-[BYTE3] format.
  if (time_gap > 10000) {
    Serial.println();
  }
  Serial.print(time_gap);
  if (repaired)
    Serial.print('#');
  else
    Serial.print(':');
  Serial.print(' ');
  Serial.print(packet & 3);
  Serial.print('-');
  Serial.print((packet & 252) >> 2);
  Serial.print('-');
  Serial.print((packet & 0xFF00) >> 8);
  if (packet > 0xFFFF) {
    Serial.print('-');
    Serial.print((packet & 0xFF0000) >> 16);
  }
  if (packet > 0xFFFFFF) {
    Serial.print('-');
    Serial.print((packet & 0xFF000000) >> 24);
  }
  Serial.println("");
}
#endif
