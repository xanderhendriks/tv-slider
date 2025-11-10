TV Slider
=========

Introduction
------------
The TV Slider was created because the only good place for a TV in our lounge area was right in front of a window. With a 65" flatscreen TV that meant that half the window was blocked all the time.
The TV Slider fixes this problem by hiding the TV in the wall when it's not in use.

Documentation
-------------
The documentation is stored in github pages: `https://xanderhendriks.github.io/tv-slider <https://xanderhendriks.github.io/tv-slider>`_ and the source files are in github: `https://github.com/xanderhendriks/tv-slider <https://github.com/xanderhendriks/tv-slider>`_

Installation
------------
The TV Slider runs on Espressif ESP32-C6-Devkitm-1 board for controlling the motor and MQTT communication.

Build the code:
  ``idf.py set-target esp32c6
  idf.py build``

Program the target:
  ``idf.py flash``

Monitor the debug output:
  ``idf.py monitor``

Configuration
-------------
The device listens to the MQTT broker as specified in the TvSliderMqtt class with the following parameters:

- MQTT_SERVER: set to 192.168.0.253
- MQTT_PORT: set to 1883

Usage
-----
This may not fit in the ESP32-C6 flash in which case the webpage will no longer be accessible.
The webpage can be access at the following url: `tv-slider:5000 <http://tv-slider:5000>`_
