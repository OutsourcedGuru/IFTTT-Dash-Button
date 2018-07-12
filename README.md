# IFTTT Dash Button
An Adafruit Huzzah ESP8266-based pushbutton for OctoPrint

## Overview

Push a big red button (BRB), pause running print job in OctoPrint. A low-power, portable BRB as a panic switch to quickly pause a job *without* having to open a browser, connect to the OctoPrint web application and then press *that* virtual button.

## Part List
* [Adafruit HUZZAH ESP8266 Breakout](https://www.adafruit.com/product/2471) $9.95
* [LITHIUM ION POLYMER BATTERY - 3.7V 150MAH](https://tinycircuits.com/products/lithium-ion-polymer-battery-3-7v-140mah) $3.95
* [Velleman VMA321](https://www.vellemanstore.com/en/velleman-vma321-1-a-lithium-battery-charging-board-for-arduino-2-pieces) Lithium Battery Charging Board for Arduino, Quantity two - $4.15
* [Philmore Push Button Red Game Switch](https://www.frys.com/product/7833309) $5.19
* [FTDI Serial TTL-232 USB Cable](https://www.adafruit.com/product/70) $17.95
* A spare two-wire header connector with red/black or white/black wires @ 3" or longer
* Soldering iron, solder, flux and a small breadboard to temporarily hold the project

In order to program the ESP8266, it will be necessary to solder a six-pin section of the included header to the short end and a small four-pin header beginning on the GND/BAT+ long end. This should make it over to GPIO 13, the config pin for this project.

When running, only the smaller header is in use with two pins going to the Velleman VMA321 charging board. The leads from the battery are soldered to this charging board as well. Make sure to get the polarity correct: connectors marked with "+" are the powered side.

![img_0049](https://user-images.githubusercontent.com/15971213/42649931-e409d3e6-85bf-11e8-8eb4-6312e994789c.jpg)

## Building and Flashing
TBD****

## Original Project Info, Before Conversion

More documentation on [Instructables](https://www.instructables.com/id/Tiny-ESP8266-Dash-Button-Re-Configurable/).

- [ESP-12E Weather Station (in development)](#esp-12e-weather-station-in-development)
    - [Planned Features (And Progress)](#planned-features-and-progress)
        - [GET Requests](#get-requests)
            - [Triggering Actions](#triggering-actions)
            - [Monitoring Battery (in progress)](#monitoring-battery-in-progress)
        - [Power Saving](#power-saving)
        - [OTA / WebUpdate (in progress)](#ota-webupdate-in-progress)
    - [Useful Links and References](#useful-links-and-references)

## Features
 - [X] GET Requests
    - [X] Trigger Action
    - [X] Monitor Battery
 - [X] Power Saving
 - [X] Reconfigurable

## About
Inspired by Bitluni's Lab. This is a tiny dashbutton. That connects to IFTTT, and saves battery. The code has been extended to make it easy to use and universally configurable without the need for re-programming. Just flash the `.bin` file from the releases page and the SPIFFS data, enter configuration mode, set it up and you are ready to go.

### GET Requests
When the button is pushed a GET request is made to a webpage.
#### Triggering Actions
Depending on the webpage, many different actions can be triggered by the button. I suggest hooking it up to the IFTTT Maker WebHooks.
#### Monitoring Battery
When a request is made, if the setting is set, the button will also pass on the battery voltage with your web request. This way you can monitor the battery's charge. The server will recieve:
 > yoururl.com/yourrequest/_?VCC_Param.=_**VCC_Voltage**

### Power Saving
To keep the button down to a small size, a small battery needs to be used. To maintain a decent battery life, the ESP is almost always in deep-sleep mode. Pressing the button resets the ESP. The ESP reboots, makes a GET request and goes back to deep-sleep.

### Configuration
You dont need to take apart your button to re-program the url or action. If you connect `GPIO_13` to `GND` during startup the button will enter configuration mode. Then you can
1. Connect to 'BRB-IFTTT' WiFi Access Point, with the password '123987654'
2. Visit http://192.168.4.1 to open the configuration page
3. After setting your values, click on the 'Save' button then the 'Restart'
![Configuration Interface](https://gangster45671.github.io/IFTTT-Dash-Button/pictures/Config.png)


