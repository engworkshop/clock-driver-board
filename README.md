# clock-driver-board
![solder parts](media/_MG_2802r.jpg)
This repository contains design files for an optional custom printed circuit board (PCB) for the [Mystery Clock](https://makerworld.com/en/models/764838) and [Hollow Clock 4 Remix](https://makerworld.com/en/models/875220) projects on [MakerWorld](https://makerworld.com/en/@EngWorkshop). This custom driver board incorporates 3 improvements. First, a dedicated real-time clock (RTC) chip, the Maxim DS3234, provides significantly greater timekeeping accuracy than the RP2040’s internal clock. The DS3234 incorporates temperature compensation to achieve its improved accuracy. Second, a dedicated stepper motor driver module provides quieter motor operation compared to the original design’s ULN2003 chip. The motor driver module incorporates the Trinamic TMC2208, which uses a “chopper drive” for quieter operation. Third, 2 pushbuttons allow fine adjustment of the hands.

The clock projects use a small, inexpensive 28BYJ-48 5V unipolar stepper motor. This motor must be converted to a bipolar configuration before it can be used with the clock driver board’s TMC2208 chip, which is designed for bipolar stepper motors. Motor conversion instructions are included at the bottom of this README file.

(All products shown or mentioned are items that I actually use and have found to work well. I have no relationship with any product supplier.)

## 1.&nbsp;&nbsp; The Circuit
![schematic](media/Clock_Driver_R3_v7.png)

**Circuit Description**

The RP2040 microcontroller communicates with the DS3234 RTC using the SPI bus. The clock chip is configured in software to send an interrupt to the microcontroller once every minute. In response to the interrupt, the microcontroller moves the minute hand through the TMC2208 motor driver. A pull-down resistor on the motor driver’s STEP input prevents spurious motor stepping during startup. A 6V boost regulator satisfies the motor driver module’s minimum supply voltage of 5.5V.

**Parts List**
- [Waveshare RP2040-Zero](https://www.waveshare.com/rp2040-zero.htm) microcontroller board without pin headers
- [TMC2208 SilentStepStick](https://www.amazon.com/gp/product/B08DFVZV5Q) stepper motor driver module
- [Pololu U3V16F6](https://www.pololu.com/product/4942) 6V boost voltage regulator
- Maxim DS3234 real-time clock chip ([DigiKey DS3234SN#-ND](https://www.digikey.com/en/products/detail/analog-devices-inc-maxim-integrated/DS3234SN/1197584))
- (2) C&K PTS645VK58-2LFS right angle tactile switches ([DigiKey CKN10965-ND](https://www.digikey.com/en/products/detail/c-k/PTS645VK58-2-LFS/3861398))
- 33 µF electrolytic capacitor (for example, [DigiKey P977-ND](https://www.digikey.com/en/products/detail/panasonic-electronic-components/ECE-A1EKS330/160557))
- 1 kΩ resistor (for example, [DigiKey 1.0KEBK-ND](https://www.digikey.com/en/products/detail/yageo/CFR-12JB-52-1K/4000))
- 0.1 µF ceramic capacitor (for example, [DigiKey 399-4264-ND](https://www.digikey.com/en/products/detail/kemet/C320C104K5R5TA/818040))
- male headers (for example, [DigiKey S1011EC-40-ND](https://www.digikey.com/en/products/detail/sullins-connector-solutions/PRPC040SAAN-RC/2775214))
- (4) M2 x 5 mm self-tapping pan head screws for mounting PCB (for example, [Amazon](https://www.amazon.com/gp/product/B07ZH9GJWP))
- [28BYJ-48 5V geared stepper motor](https://www.amazon.com/gp/product/B01CP18J4A)

## 2.&nbsp;&nbsp; Obtaining the PCB
The `board` folder contains the PCB’s Gerber files. In `upload to DKRed.zip`, I packaged the core files required by the [DKRed custom PCB](https://www.digikey.com/en/resources/dkred) service that I use. I included all Gerber files produced by Fusion 360, but I have only tested those required by DKRed:
- `copper_top.gbr` & `copper_bottom.gbr` — describes copper layers
- `soldermask_top.gbr` & `soldermask_bottom.gbr` — describes soldermask layers
- `silkscreen_top.gbr` & `silkscreen_bottom.gbr` — describes silkscreen layers
- `profile.gbr` - board outline
- `drill_1_16.xln` - drill locations

## 3.&nbsp;&nbsp; Soldering the PCB

### Step 3.1
![solder clock chip](media/_MG_2800r.jpg)
Solder the DS3234 clock chip on underside of the PCB. This is fairly easy to do with a fine-tipped soldering iron and desoldering braid (see SparkFun’s [tutorial on soldering SMD devices](https://www.sparkfun.com/tutorials/36)). No dedicated SMD soldering equipment is necessary.

### Step 3.2
![solder headers](media/_MG_2801r.jpg)
Solder male headers for the RP2040 Zero and voltage regulator. Optional: solder a female header for the motor. If you are making a Mystery Clock or Hollow Clock 4 Remix, **DO NOT SOLDER** a connector for the motor. Instead, the motor wires will need to be soldered directly to the PCB in order to fit in the clock base.

### Step 3.3
![solder parts](media/_MG_2802er.jpg)
Solder the remaining parts onto the PCB. Note that an extra header pin (circle) must be soldered to the SilentStepStick. Trim all header pins protruding on top or bottom of the PCB so they are flush with their respective solder connections. The PCB can be mounted with M2 screws.

## 4.&nbsp;&nbsp; Converting the Stepper Motor from Unipolar to Bipolar

### Step 4.1
![remove motor cover](media/_MG_3632r.jpg)
With a utility knife, score the small tab on the plastic motor cover until it breaks off. Pry open the cover.

### Step 4.2
![remove plastic tab](media/_MG_3634r.jpg)
Remove the small plastic tab that was broken off in Step 3.1. Be careful not to damage the extremely delicate motor windings.

### Step 4.3
![cut PCB trace](media/_MG_3637r.jpg)
With a utility knife, carefully cut/scrape the center PCB trace (large arrow). (See SparkFun’s [tutorial on cutting PCB traces](https://learn.sparkfun.com/tutorials/how-to-work-with-jumper-pads-and-pcb-traces/cutting-a-trace-between-jumper-pads).) The middle (red) wire is no longer functional and may be clipped off. Take note of the wire color corresponding to each coil connection.

### Step 4.4
![trim cover tabs](media/_MG_3638r.jpg)
Glue the plastic cover back on the motor. If necessary, trim the remaining tabs so that the cover can be inserted easily.

## 5.&nbsp;&nbsp; The Code

The `code` folder contains a single Arduino code file, `mystery-clock-tmc.ino`. The code’s comment block details its dependencies and operation. The following are required to install the code:

- [Arduino IDE](https://www.arduino.cc/en/software), if you do not already have it
- Earle Philhower’s [Raspberry Pi Pico RP2040 processor Arduino core](https://arduino-pico.readthedocs.io/en/latest/) using the Arduino Boards Manager
- SparkFun’s [DS3234 RTC library](https://github.com/sparkfun/SparkFun_DS3234_RTC_Arduino_Library) for Arduino

## 6.&nbsp;&nbsp; Operating Instructions

The pushbuttons move the clock hands clockwise or counterclockwise. Pushing a button momentarily causes the minute hand to move one minute. Pressing and holding a button moves the minute hand continuously.
