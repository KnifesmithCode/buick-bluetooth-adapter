# Buick Bluetooth Adapter
Based largely on findings and code from https://stuartschmitt.com/e_and_c_bus/

Planning a major re-write, but for now this works as a Bluetooth interface for any car with a GM E&C bus - see the findings from the above website for more information. I have personally tested (and implemented) the solution in my 2003 Buick Regal.

Arduino interface for communication with the Entertainment &amp; Comfort serial bus found in some GM vehicles.

The EAGLE subdirectory contains EAGLE CAD files for an interface to be used with an Arduino Pro Mini.

The ECcomm_adapter subdirectory contains the sketch to upload to the Arduino. This code could be used with other Arduino variants.

The ECcomm_adapter Arduino sketch can communicate with a serial terminal application as well as the co-developed ECcomm program for Linux (https://github.com/svschmitt/ECcomm/ ), which provides additional functionality included message decoding and bidirectional communication.
