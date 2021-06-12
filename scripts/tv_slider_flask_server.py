#!/usr/bin/env python

import json
import logging
import paho.mqtt.subscribe as subscribe
import paho.mqtt.client as mqttClient 
import RPi.GPIO as GPIO
import threading
import time

from enum import Enum
from flask import Flask, render_template


logger = logging.getLogger('tv_slider_flask_server')
logger.setLevel(logging.DEBUG)

SLOW_SPEED = 10000
FAST_SPEED = 95000

class Direction(Enum):
    IN = 1
    OUT = 2

motor_speed = 0
motor_direction = Direction.IN

def sensors_stop_callback(channel):
    global motor_speed

    print(f'sensors_stop_callback: {channel}')
    if (((motor_direction == Direction.IN) and (channel == 21)) or
        ((motor_direction == Direction.OUT) and (channel == 25))):
        print('sensors_stop_callback: stop')
        speed_set(0)

def sensors_slow_callback(channel):
    global motor_speed
    print(f'sensors_slow_callback: {channel}')
    if (((motor_direction == Direction.IN) and (channel == 20)) or
        ((motor_direction == Direction.OUT) and (channel == 16))): 
        if motor_speed > SLOW_SPEED:
            print('sensors_slow_callback: slow down')
            speed_set(SLOW_SPEED)

def mqtt_subscribe_callback(client, userdata, message):
    print("%s %s" % (message.topic, message.payload))
    mqtt_client.publish('tv-slider/state', message.payload.decode(), 0, True)

    if message.payload.decode() == 'ON':
        print('mqtt_subscribe_callback: OUT')
        move('OUT')
    else:
        print('mqtt_subscribe_callback: IN')
        move('IN')

def mqtt_thread():
    global mqtt_client
    mqtt_client.connect('192.168.0.253', 1883) 
    subscribe.callback(mqtt_subscribe_callback, 'tv-slider/switch', hostname='192.168.0.253')

GPIO.setmode(GPIO.BCM)
GPIO.setup(4, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
GPIO.setup(17, GPIO.OUT)
GPIO.setup(18, GPIO.OUT)
GPIO.setup(16, GPIO.IN)
GPIO.setup(20, GPIO.IN)
GPIO.setup(21, GPIO.IN)
GPIO.setup(25, GPIO.IN)
GPIO.output(17, GPIO.LOW)
GPIO.add_event_detect(25, GPIO.RISING, callback=sensors_stop_callback)  
GPIO.add_event_detect(16, GPIO.RISING, callback=sensors_slow_callback)  
GPIO.add_event_detect(20, GPIO.RISING, callback=sensors_slow_callback)  
GPIO.add_event_detect(21, GPIO.RISING, callback=sensors_stop_callback)  
pwm = GPIO.PWM(18, 1000)

mqtt_client = mqttClient.Client("Python") 
x = threading.Thread(target=mqtt_thread)
x.start()

app = Flask(__name__)


@app.route("/")
def index():
    return render_template("index.html", motor_speed=motor_speed, motor_direction=motor_direction)

@app.route("/move/<direction>")
def move(direction):
    direction_set(direction)
    speed_set(FAST_SPEED)
    return 'OK'

@app.route("/speed/set/<speed>")
def speed_set(speed):
    global motor_speed
    int_speed = int(speed)

    if int_speed > 0:
        pwm.ChangeFrequency(int_speed)
        pwm.start(50)
    else:
        pwm.stop()

    motor_speed = int_speed
    return 'OK'


@app.route("/speed/get")
def speed_get():
    return motor_speed


@app.route("/sensors/get")
def sensors_get():
    return '%s, %s, %s, %s' % (GPIO.input(25), GPIO.input(16), GPIO.input(20), GPIO.input(21))  


@app.route("/disable/set/<disabled>")
def disable_set(disabled):
    bool_disabled = disabled in ['1', 'True', 'true']
    
    if bool_disabled:
        GPIO.setup(4, GPIO.OUT)
        GPIO.output(4, 1) 
    else:
        GPIO.setup(4, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    return 'OK'


@app.route("/direction/set/<direction>")
def direction_set(direction):
    global motor_direction
    bool_direction = direction in ['1', 'out', 'Out', 'OUT']

    GPIO.output(17, GPIO.HIGH if bool_direction else GPIO.LOW)
    motor_direction = Direction.OUT if bool_direction else Direction.IN
    return 'OK'


@app.route("/direction/get/<direction>")
def direction_get():
    return motor_direction


@app.route("/log")
def log():
    file = open('/home/pi/aircon-controller.log')
    logs = file.read()

    return logs


def main():
    # Setup flask server
    app.run(host='0.0.0.0')

if __name__ == "__main__":
    main()
