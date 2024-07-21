# Recovery with an Arduino Uno as external programmer

## Introduction

This guide describes how you can program the SMC using an
Arduino Uno as external programmer to recover the firmware.

To follow the guide you need the following:

- An Arduino Uno
  
- A USB cable that connects the Arduino Uno to your PC
  
- Five female to male jumper cables, and one male to male jumper cable
  
- A breadboard
  
- A suitable prying tool to remove the SMC from the X16 board
  
- It is recommended that you use an anti-static wristband during the procedure to protect the circuits

Even though the risk of damaging the X16 or the SMC is small, there is as always a risk. Follow the
instructions below to minimize that risk.

## Install the Arduino IDE on your PC

Begin by downloading and installing the Arduino IDE if you do not already have it installed.

It can be downloaded from https://www.arduino.cc/en/software. This tutorial is based on version 1.8.19.

## Configure the Arduino Uno as ICSP programmer

The Arduino Uno does not by default work as a programmer. You need to upload some code to the Arduino for this to work:
      
- In the Arduino IDE, click File/Examples/11.ArduinoISP/ArduinoISP. This will open a project containing all code that we need to install.
      
- Ensure that nothing is attached to the Arduino’s pin headers.
      
- Attach a USB cable between your computer and the Arduino Uno.
      
- In the IDE, select Tools/Board/Arduino AVR boards/Arduino Uno.
      
- Verify under Tools/Port that the port you attached the Arduino Uno to is selected.
      
- Click on Sketch/Upload to upload the ArduinoISP code to your board.

## Add support for ATtiny boards

The SMC is an ATtiny861 microcontroller, which is not supported in the IDE by default. 

To add support for it, open the Preferences dialog, and add the following text to the Additional Boards Manager URLs: http://drazzy.com/package_drazzy.com_index.json.

While you are there, also tick the option Show verbose output during compilation and upload.

Go to Tools/Manage Libraries... and select ATTinyCore. Click install. If you cannot see the
ATTinyCore library, you may need to restart the IDE.

## Make a dry test upload

The command line program avrdude will be used later in this tutorial to actually program the SMC. The Arduino IDE also uses avrdude behind the scenes. The purpose of the dry test upload is to retrieve information about how avrdude is called on your system.

Follow these steps:

- Ensure that nothing is attached to the Arduino’s pin headers.

- Ensure that the Arduino is still connected to your PC with the USB cable.

- Under Tools/Board/ATTinyCore, select ATtiny261/461/861 (a). Do not select the one that ends with Optiboot.
      
- Under Tools/Chip, select ATtiny861 (a).
      
- Under Tools/Programmer, select Arduino as ISP.
      
- Under Tools/Port, verify that the port you connected the Arduino to is selected.
      
- Open File/01.Basics/Blink. This is a simple code sample that blinks a LED. It does not matter, because we not really installing this code.
      
- Click on Sketch/Upload.

If you enabled verbose output as mentioned above, you will see where the avrdude executable is stored in your system and what options are used when it is called. Copy that information from the output window to be used later on in this tutorial.

The upload will fail, as we had not connected anything to the Arduino’s ICSP header. That is fine.

## Remove the SMC from the Commander X16

Before you begin this step, it is recommended that you put on an anti-static wristband to protect the circuits from damage.

Ensure that the Commander X16 is disconnected from mains power as the SMC is powered at all times, even if the computer is turned off.

Make a note of which way the SMC is oriented, indicated by the notch in the plastic casing. You need to know that in order to reinstall the SMC the correct way.

The SMC is a microcontroller marked with ATtiny861A on top of it. Is is socketed and you can remove it using a dedicated chip puller or by carefully prying it out little by little on each side with a suitable non-scratching tool.

## Connect the Arduino Uno to the SMC on a breadboard

Before you begin this step, ensure that both the Arduino USB cable is disconnected.

Now do the following:

- Install the SMC on the breadboard.
      
- Connect jumper wires between the Arduino and the SMC as shown below. Pin 1 of the SMC is identified by a notch in the plastic casing at the top. 

| Arduino Uno   | SMC/ATtiny861A |
|---------------|----------------|
|ICSP-1 MISO	| 2-PB1/MISO     |
|ICSP-2 VCC		| 5-VCC          |
|ICSP-3 SCK     | 3-PB2/SCK      |
|ICSP-4 MOSI    | 1-PB0/MOSI     |
|ICSP-5 RST		| Not connected  |
|ICSP-6 GND		| 6-GND          |
|Digital Pin 10	| 10-PB7/RESET   |

## Download the SMC firmware

Download the SMC firmware from https://github.com/X16Community/x16-smc/releases.

You need the file that is named firmware_with_bootloader.hex. This file contains both the firmware and a bootloader that makes it possible to make further updates directly from the X16.

## Program the SMC

Programming the SMC is done as follows:

- Connect a USB cable between the Arduino and your PC. 
      
- The connection between the Arduino and the SMC on a breadboard that you made earlier should be kept in place unchanged.

Programming the SMC requires you to first set some board options that are called “fuses” in Arduino world. This is done by the following command:

```avrdude -cstk500v1 -pattiny861 -P<yourport> -b19200 -Ulfuse:w:0xF1:m -Uhfuse:w:0xD4:m -Uefuse:w:0xFE:m```

It is important that you do not change the values after -Ulfuse, -Uhfuse or -Uefuse as there is a risk of bricking the Attiny if you do so.

If any of the values for -c, -p, -P or -b was different in step ["Make a dry test upload"](#make-a-dry-test-upload) above, use that value instead.

After that you can write the actual firmware to the SMC with the following command.

```avrdude -cstk500v1 -pattiny861 -P<yourport> -b19200 -Uflash:w:firmware_with_bootloader.hex:i```

## Cleaning up

Unless there was an error message when running avrdude, you have now updated the SMC firmware. In order to finish up:

- Disconnect the USB cable between the computer and the Arduino.

- Reinstall the SMC into the X16 board. It is important that you install it the same way as it was before you removed it.

- Remove the anti-static wristband, if you used one.

- Connect mains power to the X16, and press the power button to start.
