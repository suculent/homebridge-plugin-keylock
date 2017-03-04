# homebridge-plugin-keylock

This is simple [homebridge](https://github.com/nfarina/homebridge) plugin for ESP8266 with Futaba servo.

## Prerequisites

* Arduino IDE
* ESP8266 with NodeMCU firmware
* Futaba S3003 (or any other) servo
* Homebridge on local WiFi

## Installation

```
    git clone https://github.com/suculent/homebridge-plugin-keylock.git
    cd homebridge-plugin-keylock
    npm install -g .
```

Upload the code using Arduino IDE.

Connect to the ESP8266_KEYLOCK with PASSWORD and setup to connect to your local WiFi.

Restart your ESP8266. It should start listening to your MQTT channel. You can test it by sending `UNLOCK` or `LOCK` to MQTT channel `/keylock/A` with default configuration.

Edit your Homebridge configuration based on sample-config.json file.

Restart your homebridge and add the new device.
