#!/usr/bin/env python

import logging
import sys
import tv_slider_motor_control
import tv_slider_mqtt

from flask import Flask, Response, request
from flask_cors import CORS
from flask_sse import sse
from redis_logging import RedisLoggingHandler

app = Flask(__name__, static_folder='/home/pi/tv-slider/react-flask-app/build', static_url_path='/')
CORS(app)
app.config["REDIS_URL"] = "redis://localhost:6379"
app.register_blueprint(sse, url_prefix='/stream')

# configure logging
logging_formatter = logging.Formatter('%(asctime)s %(levelname)s %(name)s %(threadName)s : %(message)s')

file_logging_handler = logging.FileHandler('test_log.txt')
file_logging_handler.setFormatter(logging_formatter)
redis_logging_handler = RedisLoggingHandler(app)
redis_logging_handler.setFormatter(logging_formatter)
std_err_logging_handler = logging.StreamHandler()
std_err_logging_handler.setFormatter(logging_formatter)

redis_logging = logging.getLogger()
redis_logging.setLevel(logging.DEBUG)
redis_logging.addHandler(file_logging_handler)
redis_logging.addHandler(std_err_logging_handler)
redis_logging.addHandler(redis_logging_handler)


def direction_string_to_direction(direction_string):
    bool_direction = direction_string in ['1', 'out', 'Out', 'OUT']
    direction = tv_slider_motor_control.Direction.OUT if bool_direction else tv_slider_motor_control.Direction.IN

    return direction


@app.route("/")
def index():
    return app.send_static_file('index.html')


@app.route("/api/move/<direction>")
def move(direction):
    motor_control.move(direction_string_to_direction(direction))
    return 'OK'


@app.route("/api/move/stop")
def stop():
    motor_control.stop()
    return 'OK'


@app.route("/api/speed/set/<speed>")
def speed_set(speed):
    motor_control.speed_set(int(speed))
    return 'OK'


@app.route("/api/speed/get")
def speed_get():
    speed = motor_control.speed_get()
    return f'Speed: {speed}'


@app.route("/api/sensors/get")
def sensors_get():
    sensors = motor_control.sensors_get()
    redis_logging.info(f'sensors {sensors}')
    return f'Sensors: {sensors}'


@app.route("/api/direction/set/<direction>")
def direction_set(direction):
    motor_control.direction_set(direction_string_to_direction(direction))
    return 'OK'


@app.route("/api/direction/get")
def direction_get():
    direction = 'OUT' if motor_control.direction_get() == tv_slider_motor_control.Direction.OUT else 'IN'

    return f'Direction: {direction}'


@app.route("/api/log")
def log():
    file = open('/home/pi/tv-slider.log')
    logs = file.read()

    return logs


def mqtt_callback(direction):
    redis_logging.info(f'mqtt_callback {direction}')

    motor_control.move(direction_string_to_direction(direction))


motor_control = tv_slider_motor_control.TvSliderMotorControl()
mqtt = tv_slider_mqtt.TvSliderMqtt(mqtt_callback)
