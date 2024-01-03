# Buick Bluetooth Adapter
Based largely on findings and code from https://stuartschmitt.com/e_and_c_bus/

I am planning a major re-write, but for now this works as a Bluetooth interface for any car with a GM E&C bus - see the findings from the above website for more information. I have personally tested (and implemented) the solution in my 2003 Buick Regal.

The "Fusion 360" folder contains the CAD files which I am using.

The EComm_adapter contains the ESP 32 project and code for both the communication and Bluetooth adapter.

### TODO:
- Improve communication reliability
  - Experiment with interrupts instead of polling (interrupts would be potentially much more reliable)
- Implement steering-wheel (and head unit) button control
- Fix timing so that the head unit displays the correct time for a song (may be impossible due to changing song lengths)
- Allow fast-forwarding from the head unit (again, may be impossible due to changing song lengths)
