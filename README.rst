TV Slider
=========

Introduction
------------
The TV Slider was created because the only good place for a TV in our lounge area was right in front of a window. With a 65" flatscreen TV that meant that half the window was blocked all the time.
The TV Slider fixes this problem by hiding the TV in the wall when it's not in use.

Locations
^^^^^^^^^
The documentation is stored in github pages: `https://xanderhendriks.github.io/tv-slider <https://xanderhendriks.github.io/tv-slider>`_ and the source files are in github: `https://github.com/xanderhendriks/tv-slider <https://github.com/xanderhendriks/tv-slider>`_

Installation
------------
The TV Slider runs on a standard RaspOS Lite image. Before inserting the SD Card in the RPi create an empty file with the name ssh in the boot drive to enable ssh.

Once started up connect to the device and change the hostname to **tv-slider** by replacing **raspberrypi** in the following files:

- /etc/hosts
- /etc/hostname

Having a static IP can be convenient to find the device on the LAN and this can be done by updating the **/etc/dhcpcd.conf**. The **Example static IP configuration** section in the file shows how.

execute the following commands:

1. Install GIT, Python PIP and virtual environment:
  ``sudo apt install git python3-pip python3-venv``

2. Clone the repo in the **/home/pi** directory:
  ``git clone git@github.com:xanderhendriks/tv-slider.git``

3. Install the required Python packages: 
  ``pip3 install -r scripts/requirements.txt``

4. Install nodejs 18: 
  ``curl -sL https://deb.nodesource.com/setup_18.x | sudo -E bash -
  sudo apt-get install -y nodejs``

5. Install yarn:
  ``curl -sS https://dl.yarnpkg.com/debian/pubkey.gpg | sudo apt-key add -
  echo "deb https://dl.yarnpkg.com/debian/ stable main" | sudo tee /etc/apt/sources.list.d/yarn.list
  sudo apt install -y yarn
  yarn --version``

6. Build the code:
  ``cd react-flask-app
  yarn install
  yarn start``

7. Create a symbolic link for the service: 
  ``sudo ln -s /home/pi/tv-slider/tv-slider.service /etc/systemd/system/tv-slider.service``

Configuration
-------------
The device listens to the MQTT broker as specified in the TvSliderMqtt class with the following parameters:

- MQTT_SERVER: set to 192.168.0.253
- MQTT_PORT: set to 1883

Usage
-----
Start the service with the following command:

``sudo systemctl start tv-slider``

Now the device can be access at the following url: `tv-slider:5000 <http://tv-slider:5000>`_
