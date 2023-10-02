# esp32-eink-sat-timerstatus
Display active FM sats on eInk display using DF2ET Satellite ⏳ Timer Status API

![img/eInk-4.2.png](https://github.com/ok1cdj/esp32-eink-sat-timerstatus/blob/e5d80a59d59acc3304756397fb83a28e1c8db0a0/img/eInk-4.2.png)

Base on 4.2" eInk and ESP32  from laskakit [https://www.laskakit.cz/laskakit-espink-42-esp32-e-paper-pcb-antenna/](https://www.laskakit.cz/laskakit-espink-42-esp32-e-paper-pcb-antenna/)

Can be portred on *another boards* for sure. PR welcome.

Code is developed in Arduino.

### Configuration 
Copy `config.txt` to SD card and edit WiFi SSID and KEY.

### Compilation
Select Wrower modelule in Arduino IDE

You need some libraries installed:

`Arduino JSON`

`GxEPD2`

`ESPAsyncWebServer`

`AsyncElegantOTA`


### Binary install
You can download package and use ESP32 tool for upload to your board. 

