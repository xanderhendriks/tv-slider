#!/usr/bin/env python

import logging
import sys
import tv_slider_motor_control
import tv_slider_mqtt

from flask import Flask, render_template


logger = logging.getLogger(__name__)

logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

app = Flask(__name__)

motor_control = None


def direction_string_to_direction(direction_string):
    bool_direction = direction_string in ['1', 'out', 'Out', 'OUT']
    direction = tv_slider_motor_control.Direction.OUT if bool_direction else tv_slider_motor_control.Direction.IN

    return direction

@app.route("/")
def index():
    return render_template("index.html", motor_speed=0, motor_direction=0)


@app.route("/move/<direction>")
def move(direction):
    direction = motor_control.move(direction_string_to_direction(direction))
    return 'OK'


@app.route("/speed/set/<speed>")
def speed_set(speed):
    motor_control.speed_set(int(speed))    
    return 'OK'


@app.route("/speed/get")
def speed_get():
    speed = motor_control.speed_get()
    return f'Speed: {speed}'


@app.route("/sensors/get")
def sensors_get():
    sensors = motor_control.sensors_get()
    return f'Sensors: {sensors}'


@app.route("/direction/set/<direction>")
def direction_set(direction):
    motor_control.direction_set(direction_string_to_direction(direction))
    return 'OK'


@app.route("/direction/get")
def direction_get():
    direction = 'OUT' if motor_control.direction_get() == tv_slider_motor_control.Direction.OUT else 'IN' 

    return f'Direction: {direction}'


@app.route("/log")
def log():
    file = open('/home/pi/tv-slider.log')
    logs = file.read()

    return logs


def mqtt_callback(direction):
    logger.info(f'mqtt_callback {direction}') 

    motor_control.move(direction_string_to_direction(direction))    


def main():
    global motor_control

    motor_control = tv_slider_motor_control.TvSliderMotorControl()
    mqtt = tv_slider_mqtt.TvSliderMqtt(mqtt_callback)
    app.run(host='0.0.0.0')

    mqtt.stop()


if __name__ == "__main__":
    main()
