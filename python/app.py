#!/usr/bin/env python

import logging
import signal
import sys
import tv_slider_motor_control
import tv_slider_mqtt

from flask import Flask
from flask_cors import CORS
from flask_sse import sse
from sse_logging import SseLoggingHandler


# configure logging
logging_formatter = logging.Formatter('%(asctime)s %(levelname)s %(name)s %(threadName)s : %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

file_logging_handler = logging.FileHandler('test_log.txt')
file_logging_handler.setFormatter(logging_formatter)
std_err_logging_handler = logging.StreamHandler()
std_err_logging_handler.setFormatter(logging_formatter)

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
logger.addHandler(file_logging_handler)
logger.addHandler(std_err_logging_handler)

app = Flask(__name__, static_folder='/home/pi/tv-slider/react-flask-app/build', static_url_path='/')
CORS(app)

app.config["REDIS_URL"] = "redis://localhost:6379"
app.register_blueprint(sse, url_prefix='/stream')

# Add logging across the SSE link to the browser
sse_logging_handler = SseLoggingHandler(app)
sse_logging_handler.setFormatter(logging_formatter)
logger.addHandler(sse_logging_handler)


def handle_signal(signum, frame):
    logger.info(f'handling signal {signum}')

    motor_control.end()
    mqtt.end()

    exit(0)


signal.signal(signal.SIGINT, handle_signal)


def position_callback(position):
    logger.info(f'position_callback: {position}')
    with app.app_context():
        sse.publish(str(position), type='position')


def mqtt_callback(direction):
    logger.info(f'mqtt_callback: {direction}')

    motor_control.move(direction_string_to_direction(direction))

if len(sys.argv) > 1 and sys.argv[1] == "run":
    motor_control = tv_slider_motor_control.TvSliderMotorControl(position_callback)
    mqtt = tv_slider_mqtt.TvSliderMqtt(mqtt_callback)


def direction_string_to_direction(direction_string):
    bool_direction = direction_string in ['1', 'out', 'Out', 'OUT']
    direction = tv_slider_motor_control.Direction.OUT if bool_direction else tv_slider_motor_control.Direction.IN

    return direction


@app.route("/")
def index():
    return app.send_static_file('index.html')


@app.route("/api/test/<position>")
def test(position):
    sse.publish(str(position), type='position')
    return 'OK'


@app.route("/api/move/<direction>")
def move(direction):
    motor_control.move(direction_string_to_direction(direction))
    return 'OK'


@app.route("/api/move/stop")
def stop():
    motor_control.stop()
    return 'OK'


@app.route("/api/position/get")
def position_get():
    position = motor_control.position_get()
    logger.info(f'position_get {position}')

    return f'{position}'


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
