# Android USB Audio Device
This project enables piping audio from a Windows PC to an Android device.
## Why?
I wanted to use my phone as a Bluetooth audio transmitter because I wasn't quite satisfied with the other options:  
* Connecting my headphones straight to my PC
  * Windows supports only SBC, AAC, and plain aptX for Bluetooth audio
  * Manually switching Bluetooth connection between my phone and PC
* External Bluetooth transmitter (Fiio BTA 30)
  * USB is limited to 48 KHz 16-bit aptX HD
  * My current PC doesn't have SPDIF which allows 96 KHz 24-bit LDAC
  * Does not forward inline media controls to the PC
  * Manually switching Bluetooth connection between my phone and PC
## How well does it work?
In my configuration, the audio quality is as solid as something running directly on the Android device. Latency is noticably worse than BTA 30 if you're paying attention, but in my opinion is still low enough for video. It's a little bit more noticable in games.  
Your mileage may vary since the Android USB accessory protocol only allows bulk "slow" transfers. So if something on the same USB controller or bus is using a higher priority transfer then the audio may hiccup.
# Installation
## The easy part
Install the APK on your android device.
## Install virtual audio device
Writing a Windows audio driver looked like a nightmare so I decided to just use a driver that forwards audio from an output device to an input device.
I used the "HiFi-Cable" found here:
https://vb-audio.com/Cable/  
Configure both ends of the cable to the desired sample rate and depth (I only tested 48 and 96 KHz, Android theoretically supports up to 192 KHz) and ensure the output allows exclusive mode.  
![HiFi Cable Properties](ReadmeResources/HiFi%20Cable%20Properties.png)  
Optionally, open the ASIO Bridge application that came with HiFi Cable as an administrator, and change the max latency to 4096 samples. In theory this could increase audio glitches but I didn't notice any at 96 Khz. If you don't open as an administrator this will silently fail to change.
![HiFi Cable Control Panel](ReadmeResources/HiFi%20Cable%20Control%20Panel.png) 
## Install USB drivers
By default, Android devices don't have generic WinUSB drivers which are needed for the application to communicate with it. Download Zadig from https://zadig.akeo.ie/.Zadig generates and installs generic USB drivers for a device.
First, connect the Android device and ensure it is in "Charging this device via USB"/"No Data Transfer Mode". Then open Zadig, and check `Options > List All Devices`. Select your device in the main dropdown, and select the WinUSB driver, then click Replace Driver.
![Zadig Replace Driver](ReadmeResources/Zadig%20Replace%20Driver.png)
Next, run the Windows application. The Android device should prompt you for permission to allow the USB audio service to handle the USB connection. Accept, and then watch the Windows application panic because it can't find your device. When the Android device is in accessory mode, it presents itself to Windows as a different device, so you'll need to follow the steps to install the USB driver a second time.  
After that, make sure the virtual cable is set as your audio output device in Windows, and relaunch the Windows application. At this point, it should connect successfully and audio should be routed to the Android device.