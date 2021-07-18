TV Slider
=========

Introduction
------------
The TV Slider was created because the only good place for a TV in our lounge area was right in front of a window. With a 65" flatscreen TV that meant that half the window was blocked all the time.
The TV Slider fixes this problem by hiding the TV in the wall when it's not in use.

Installation
------------
The TV Slider runs on a standard RaspOS Lite image. Before inserting the SD Card in the RPi create an emty file with the name ssh in the boot drive to enable ssh.

Once started up connect to the device and change the hostname to **tv-slider** by replacing **raspberrypi** in the following files:

- /etc/hosts
- /etc/hostname

Having a static IP can be convenient to find the device on the LAN and this can be done by updating the **/etc/dhcpcd.conf**. The **Example static IP configuration** section in the file shows how.

clone the repo in the **/home/pi** directory and execute the follwing commands:

1. Install the required Python packages: ``pip3 install -r scripts/requirements.txt``
2. Create a symbolic link for the service: ``sudo ln -s tv-slider/linux/etc/systemd/system/tv-slider.service /etc/systemd/system/tv-slider.service``

Configuration
-------------
The device listens to the MQTT broker as specified in the TvSliderMqtt class with the follwoing parameters:

- MQTT_SERVER: set to 192.168.0.253
- MQTT_PORT: set to 1883

Usage
-----
Start the service with the following command:

``sudo systemctl start tv-slider``

Now the device can be access at the following url: `tv-slider:5000 <http://tv-slider:5000>`_
