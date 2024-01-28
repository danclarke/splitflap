# Split Flap

TODO: Picture here

Split Flap is based on Split-Flap Display by David Kingsman:

3D Model: https://www.printables.com/model/69464
GitHub: https://github.com/Dave19171/split-flap

The 3D printed parts are largely the same except for a few tweaks. The electronics and code have been completely replaced wholesale.
The original model uses an Arduino in each module, and an ESP8266 to control them all via I2C.
I've replaced the primary control with an ESP32. This is then connected directly to the stepper motors via 74HC595 shift registers. Two motors are connected to a PCB containing one shift register. Extension is therefore still possible by daisy chaining more PCBs and motors. Ideally the hall sensors would connect to a multiplexer (parallel in, serial out) for maximum modularity. Instead for convenience I've connected the hall sensors to pins directly on the ESP32 since it has plenty of pins spare in this implementation.

This guide provides full detail on the project. To recreate your own, you can follow the quick guide. However, I would recommend reading through this document first to gain a full understanding of the project.

# PCB Manufacturing

There are two separate PCBs, they can be found within 'hardware'. The primary PCB is the 'Flaps Controller', one of these PCBs are required. 'Motors Controller' is used for connection directly to the stepper motors and hall sensors, one of these is required for every two split flap units/modules.

The PCBs are specifically designed to be hand solderable. However, this is still SMT soldering and requires some experience to perform successfully. Fortunately, the PCBs can be manufacturered and populated by JLCPCB. The voltage regulator is a standard LM2596S module readily available: https://www.bitsboxuk.com/index.php?main_page=product_info&cPath=140_171&products_id=3202

The voltage regulator must be hand soldered to the 'Flaps Controller'. **Make sure the output voltage is correctly set to 3.3v prior to soldering**. I've found the easiest way is to place the module on the PCB and secure. Then put the soldering iron through the in/out holes and feed a significant amount of solder. Once the solder starts to rapidly disappear, both sides have sufficient heat and the solder has bridged the connection.

# Setup

TODO

# Configuration

## Split Flap Configuration

Primary configuration is via menuconfig. Enter the values required for the Split Flap within the 'Split Flap'.

If you haven't gone for the basic English character set, you'll need to update the characters within letters.hpp.

The standard character set is:

' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '¨A', ' ¨O', '¨U', '0', '1', '2', '3', '4', '5', '6',
'7', '8', '9', ':', '.', '-', ' ?', ' !'

## ESP32 Configuration

Required settings for the ESP32 configuration within menuconfig are:

### Partition Table

Must be set to custom, with the file name 'partitions.csv' and offset 0x8000.

# API

TODO

# MQTT

TODO
