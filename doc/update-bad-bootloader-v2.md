# SMC update with bad bootloader v2 (July 13th)
Author: Eirik Stople

NB: Please read and understand the guide before attempting to use it to recover the SMC.


## Simplified explaination and guide

### Affected boards

If you have not updated your SMC yet, and your board is between PR00100 and PR00900, chances are high that you have a "bad bootloader" installed. If this is the case, you will brick your SMC (make it stop working) if you try to update it using the X16. If you do brick your SMC, you have to pop it from the machine and use an external programming tool / Arduino to recover it. See these links for a guide: (or ask for help on Discord)
- https://discord.com/channels/547559626024157184/1224366756320247942/1224779003601096826
- https://discord.com/channels/547559626024157184/1224465757786865694
- https://github.com/X16Community/x16-docs/pull/199
- https://github.com/X16Community/x16-docs/blob/83455c5664317dbc9f6a7b5662b6a1b997c14c60/X16%20Reference%20-%20Appendix%20G%20-%20SMC%20Update%20and%20Recovery.md


### Before you update

If you follow this guide, you will most likely be able to update the SMC without bricking it and having to use an external tool. But you do have to use a small patch wire.

WARNING: Although this guide explains how to perform this update, there is still a risk of something failing, causing a bricked SMC (requiring an external programmer) or even a shorted X16, in case wrong pins are connected. Proceed at your own risk.

This procedure have only been tested with R47 ROM and VERA. Enter "HELP" and ensure that you have these versions:
- COMMANDER X16 ROM RELEASE R47
- VERA: V47.0.2

If you have an older version (e.g. R45), start upgrading to R47. See here for a guide: 
- https://www.facebook.com/groups/CommanderX16/posts/1585423168875438/ or 
- https://x16community.github.io/faq/articles/r47%20update%20guide.html

IMPORTANT: DO NOT DOWNLOAD AND UPDATE SMC FIRMWARE AT THIS STEP!

Once VERA and ROM have been updated to R47, type "HELP" and note the SMC version. You can also poll the bootloader version using "PRINT I2CPEEK($42,$8E)". If it shows "2", you may either have the good or the bad v2 bootloader. You may want to take a picture of the versions you have before you proceed. The bad bootloader seem to at least be present in PR boards between PR00100 and PR00831, with SMC version 45.1.0. From around PR00900, X16s are delivered with 47.0.0, likely with a good bootloader.


### Update procedure

Bootloader version 2 is supposed to update the SMC, and power off automatically once done. The bad bootloader v2 will not power off. If you attempt to fix this by physically disconnecting the power supply, your smc WILL DEFINITELY BECOME BRICKED once you power it on again. But, if you instead temporarily connect the SMC reset pin to GND, you will restart the SMC (and power off the PSU), and the SMC will be successfully updated.

Unfortunately, the reset pin of the SMC is not connected anywhere on PR boards, so you cannot use a jumper for this task. You have to get a short patch wire to do this task, e.g. a male to female breadboard wire. Which you will have to use while the X16 is powered on. NB: You may short-circuit components on the board if you accidentally touch wrong parts / wrong pins on the live board. To do this, you need a steady hand, and have to be accurate.

You should practice this operation before using it during an actual update. Start by identifying what to connect where (see the attached images). Locate the SMC chip. It is a thin, socketed IC which have 2 x 10 pins at the bottom right of the mainboard. Below it is the text "System Microcontroller", and above it "U8". Ensure that there is a little notch on the top center of the chip, and on the top of the socket. The reset pin is pin 10, which is the bottom left. In order to activate reset, you have to provide GND to this pin. If you have a male to female jumper wire, you can use the GND on the bottom right of the mainboard (any pin mentioning GND). If you have a male to male jumper wire, you can use the metal on the screw holes of the main board.

SMC reset procedure:
- Power on the machine
- Ensure that one end of the wire have good connection to GND, without touching anything else made of metal. Make sure the other end does not touch anything.
- While the first end is connected to GND, let the tip of the other end touch the reset pin of the SMC (bottom left). Make sure it only touches the reset pin and nothing else. This causes the SMC to reset, which also causes the PSU to power off, as if you had used the power button.
- Disconnect the wire from the reset pin, and disconnect the other end from GND.

Ensure that you are accurate when you tap the reset pin on the SMC. Give it a short tap, max 1 second. You can remove the wire once the power supply turns off.

Power on the machine again and practice it until you feel comfortable doing the operation.

Once you are ready, download SMC release 47.2.3. This version contains functionality for replacing the bootloader with a good one.
SMC download: https://github.com/X16Community/x16-smc/releases/tag/r47.2.3

Extract the zip file. Locate "x16-smc.ino.hex", and copy it to the SD card.

Download "SMCUPDATE-2.0.PRG" here: https://github.com/stefan-b-jakobsson/x16-smc-update/releases/tag/2.0

Copy "x16-smc.ino.hex" and "SMCUPDATE-2.0.PRG" to SD-card.

On this page, you can see a video of what to expect on screen during the update: https://github.com/stefan-b-jakobsson/x16-smc-update

Start your X16. Start the update program using the command
- LOAD "SMCUPDATE-2.0.PRG"
- RUN

Make note of reported bootloader version, it should be 1 or 2. Read the warning and press Y. Enter filename, "x16-smc.ino.hex", without quotes. Follow the on-screen instructions to start programming, using a button combination in the process. Once programming is done, you get a countdown before the X16 will "automatically power off". If the X16 automatically powers off, you have the good v2 bootloader already. If the machine does not power off, you either have bootloader 1, or a bad bootloader 2. If bootloader is version 2 and it does not power off, DO NOT disconnect power. Instead, you have to reset the SMC by connecting a wire from ground to the reset pin, as you have already practiced earlier. If, however, bootloader is reported to be version 1, you can power the machine off with either a wire, or by disconnecting its power.

Push the power button to power on the SMC. Type "HELP" to check if you have updated to the newer version.

If this worked, you now have a method of updating your SMC without needing an external programmer. However, you may want to repair the bootloader, so that you do not have to use a wire every time there is an update. The new SMC version contains an interface which can be used to repair the bootloader. See the attached folder "smc-flash-manipulation-5" for details.


### Images

1: Overview
![1-overview](https://github.com/user-attachments/assets/32d6b78e-199e-4f2e-b331-6e8da4597f05)

2: Reset pin, GND pin and screw hole highlighted
![2-highlighted pins](https://github.com/user-attachments/assets/b4d28823-d1ba-4407-8f1a-5a6e9a705ba8)

3: Notch, ensure that the chip is oriented with the notch upwards
![3-notch](https://github.com/user-attachments/assets/0ff98684-9946-47a4-b297-52aafb07772a)

4: Option 1, male to male jumper wire: One end on the metal around the screw hole, one end on the reset pin
![4-male-to-male](https://github.com/user-attachments/assets/0de57b8b-59ee-41ac-bb08-35aaeaad4282)

5: Option 2, male to female jumper wire: Female on a GND pin (lower right), male on the reset pin
![5-male-to-female](https://github.com/user-attachments/assets/667dc564-631a-406f-9f4c-4e450e2db79a)

## Technical details below, for those interested

The first official firmware release after PR boards were shipped out was announced March 30th (r47, https://www.facebook.com/groups/CommanderX16/posts/1585423168875438/ ). Shortly after, many X16 users reported that the SMC stopped working after the update. To make the X16 usable again, they had to pop the SMC and program it with a PC and an external programmer / Arduino. As many users reported to have problems, users were encouraged to NOT update the SMC to avoid bricking it.

There is a small, separate software installed on the SMC called the "Bootloader". This program is responsible for updating the main program of the SMC. Most production versions of X16 is shipped with bootloader version 2. Bootloader version 2 will update all of the SMC program, except the first 64 bytes, which is kept in ram. Once the rest of the program is updated, the SMC restarts (using a hardware watchdog and an infinite loop), in order to reset all hardware. Then it jumps to the bootloader again, to program the first 64 bytes from ram, and then it jumps to the main program. Which will power off the PSU and the machine.

Bootloader version 2 is stable and works quite good. However, due to a mistake when preparing the firmware file which was used for initial programming of production boards ("x16-smc-r45.1-bootloader.hex"), an incorrect version of bootloader version 2 was used, for a significant amount of production machines. This incorrect bootloader does not activate the watchdog before the infinite loop. This causes that the machine is stuck in the bootloader, while the PSU is powered on. Ref https://discord.com/channels/547559626024157184/1224366756320247942/1225169489662836908 , many SMCs were loaded with 45.1.0 firmware and the corrupt bootloader. This exact file, with bad bootloader, is confirmed in PR00102, PR00129, PR00147 and PR00831. These boards failed after programming, with the same bad bootloader: PR00100 and PR00638. Ref the thread https://discord.com/channels/547559626024157184/1224366756320247942 .

If the SMC is in this state, and the user attempts to fix the problem by physically disconnecting the power, the SMC will no longer be stuck in the infinite loop. However, when the machine starts up again after restoring power, the SMC will still jump to bootloader, and write the first 64 bytes from ram. However, as the SMC have lost its power in the meantime, the contents of the RAM is gone. Thus, the first 64 bytes of flash will be programmed with 64 random bytes. Which, in all likelyhood, causes the SMC to be bricked, and requires a recovery using an external programmer.

There is another way to reset the SMC, which does not cut the power to the RAM. The SMC have a dedicated reset pin, which is active low. If you give a short low-pulse (GND) to this pin, the SMC will reset, and restart, jump to bootloader, and install the correct 64 bytes.

The bootloader will not be updated when using it to update the main program. Thus, you have to fix it somehow, if you want to avoid using a wire every time you want to update. SMC version 47.2.0 and newer contains an interface to read and write bootloader flash.


Changelog:

- Prelim 1: Version presented in pull request #38, May 31st.
- Prelim 2: Suggesting to use an updated version of SMCUPDATE.PRG, with some usage instructions. Recommending version 47.2.2 or later. Minor improvements to readme. Updated smc-flash-manipulation subfolder with version recommendation.
- Version 1: Referring to SMC release 47.2.3, SMCUPDATE version 2, and highlighting that the procedure have only been tested with ROM and VERA release R47. July 13th
