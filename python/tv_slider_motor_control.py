import logging
import RPi.GPIO as GPIO
import threading
import time

from enum import Enum


logger = logging.getLogger(__name__)


class Direction(Enum):
    IN = 0
    OUT = 1


class Position(Enum):
    UNKNOWN = 0
    IN_END = 1
    IN_SLOW = 2
    CENTER = 3
    OUT_SLOW = 4
    OUT_END = 5


class State(Enum):
    STOPPED = 0
    OUT_RAMP_UP = 1
    OUT = 2
    OUT_RAMP_DOWN = 3
    IN_RAMP_UP = 4
    IN = 5
    IN_RAMP_DOWN = 6


class TvSliderMotorControl:
    SENSOR_STRINGS = {25: 'OUT_END', 16: 'OUT_SLOW', 20: 'IN_SLOW', 21: 'IN_END'}

    SLOW_SPEED = 8000
    FAST_SPEED = 30000
    SPEED_RAMP_UP_STEP = 2500
    SPEED_RAMP_DOWN_STEP = 2500

    GPIO_SENSOR_OUT_END = 25
    GPIO_SENSOR_OUT_SLOW = 16
    GPIO_SENSOR_IN_SLOW = 20
    GPIO_SENSOR_IN_END = 21

    GPIO_MOTOR_DISABLE = 27
    GPIO_MOTOR_DIRECTION = 17
    GPIO_MOTOR_PWM = 18

    def __init__(self, callback=None):
        self.tv_position = Position.UNKNOWN
        self.motor_speed = 0
        self.motor_direction = Direction.IN
        self.drive_state = State.STOPPED
        self.callback = callback

        # A lock for the process state function to allow both GPIO callback or thread to call the function
        self.lock = threading.Lock()

        # Instruct the GPIO module to interpret pin numbers as BCM numbers
        GPIO.setmode(GPIO.BCM)

        # The configuration is still kept from the previous execution
        GPIO.setwarnings(False)

        # Configure the outputs going to the motor driver
        GPIO.setup(self.GPIO_MOTOR_DISABLE, GPIO.OUT)
        GPIO.setup(self.GPIO_MOTOR_DIRECTION, GPIO.OUT)
        GPIO.setup(self.GPIO_MOTOR_PWM, GPIO.OUT)
        self.motor_pwm = GPIO.PWM(self.GPIO_MOTOR_PWM, 1000)

        # Match the GPIO up with the internal state
        self.direction_set(Direction.IN)
        self.speed_set(0)

        # Configure the inputs from the hall sensors
        GPIO.setup(self.GPIO_SENSOR_OUT_SLOW, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(self.GPIO_SENSOR_OUT_END, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(self.GPIO_SENSOR_IN_SLOW, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(self.GPIO_SENSOR_IN_END, GPIO.IN, pull_up_down=GPIO.PUD_UP)

        # Hook the sensors up to event handlers. For the slow sensors only rising. For the end sensors both
        # rising and falling as there is a state change when it arrives and when it leaves this position
        GPIO.add_event_detect(self.GPIO_SENSOR_OUT_SLOW, GPIO.RISING, callback=self.sensors_callback)
        GPIO.add_event_detect(self.GPIO_SENSOR_OUT_END, GPIO.BOTH, callback=self.sensors_callback)
        GPIO.add_event_detect(self.GPIO_SENSOR_IN_SLOW, GPIO.RISING, callback=self.sensors_callback)
        GPIO.add_event_detect(self.GPIO_SENSOR_IN_END, GPIO.BOTH, callback=self.sensors_callback)
        GPIO.output(self.GPIO_MOTOR_DISABLE, 1)
        if GPIO.input(self.GPIO_SENSOR_OUT_END) == GPIO.HIGH:
            self.tv_position = Position.OUT_END
        elif GPIO.input(self.GPIO_SENSOR_IN_END) == GPIO.HIGH:
            self.tv_position = Position.IN_END

        logger.info(f'Initial tv_position: {self.tv_position}')

        self.report_position()

        self.thread = threading.Thread(target=self.thread_process)
        self.thread.start()

    def report_position(self):
        if self.callback is not None:
            self.callback(self.position_get())

    def end(self):
        self.thread_run = False
        self.thread.join()
        GPIO.cleanup()
        logger.info('Thread process stopped')

    def stop(self):
        self.set_state(State.STOPPED)

    def move(self, direction):
        if direction == Direction.OUT:
            logger.info('move: OUT')
            self.set_state(State.OUT_RAMP_UP)
        else:
            logger.info('move: IN')
            self.set_state(State.IN_RAMP_UP)

    def speed_set(self, speed):
        logger.debug(f'speed_set: {speed}')
        if speed > 0:
            self.motor_pwm.ChangeFrequency(speed)
            self.motor_pwm.start(50)
            self.motor_speed = speed
            GPIO.output(self.GPIO_MOTOR_DISABLE, GPIO.LOW)
        else:
            self.motor_pwm.stop()
            self.motor_speed = 0
            GPIO.output(self.GPIO_MOTOR_DISABLE, GPIO.HIGH)

    def speed_get(self):
        return self.motor_speed

    def direction_set(self, direction):
        if direction == Direction.OUT:
            GPIO.output(self.GPIO_MOTOR_DIRECTION, GPIO.HIGH)
            logger.info('direction_set: OUT')
        else:
            GPIO.output(self.GPIO_MOTOR_DIRECTION, GPIO.LOW)
            logger.info('direction_set: IN')

        self.motor_direction = direction

    def direction_get(self):
        return self.motor_direction

    def sensors_get(self):
        return (GPIO.input(self.GPIO_SENSOR_OUT_END), GPIO.input(self.GPIO_SENSOR_OUT_SLOW),
                GPIO.input(self.GPIO_SENSOR_IN_SLOW), GPIO.input(self.GPIO_SENSOR_IN_END))

    def position_get(self):
        percentage = -1

        if self.tv_position == Position.IN_END:
            percentage = 0
        elif self.tv_position == Position.IN_SLOW:
            percentage = 25
        elif self.tv_position == Position.CENTER:
            percentage = 50
        elif self.tv_position == Position.OUT_SLOW:
            percentage = 75
        elif self.tv_position == Position.OUT_END:
            percentage = 100

        return percentage

    def sensors_callback(self, channel):
        previous_position = self.tv_position

        logger.debug(f'sensors_callback: {self.SENSOR_STRINGS[channel]}')

        if channel == self.GPIO_SENSOR_OUT_END:
            if GPIO.input(self.GPIO_SENSOR_OUT_END) == 1:
                self.tv_position = Position.OUT_END
            else:
                self.tv_position = Position.OUT_SLOW

        elif channel == self.GPIO_SENSOR_OUT_SLOW:
            if self.tv_position == Position.OUT_SLOW:
                self.tv_position = Position.CENTER
            elif self.tv_position == Position.CENTER:
                self.tv_position = Position.OUT_SLOW

            # Unknown state, check direction
            elif self.motor_direction == Direction.IN:
                self.tv_position = Position.CENTER
            else:
                self.tv_position = Position.OUT_SLOW

        elif channel == self.GPIO_SENSOR_IN_SLOW:
            if self.tv_position == Position.IN_SLOW:
                self.tv_position = Position.CENTER
            elif self.tv_position == Position.CENTER:
                self.tv_position = Position.IN_SLOW

            # Unknown state, check direction
            elif self.motor_direction == Direction.OUT:
                self.tv_position = Position.CENTER
            else:
                self.tv_position = Position.IN_SLOW

        if channel == self.GPIO_SENSOR_IN_END:
            if GPIO.input(self.GPIO_SENSOR_IN_END) == 1:
                self.tv_position = Position.IN_END
            else:
                self.tv_position = Position.IN_SLOW

        logger.info(f'tv_position changed from: {previous_position} to {self.tv_position}')
        self.report_position()

        self.run_state_machine()

    def set_state(self, state):
        if state == State.STOPPED:
            self.speed_set(0)
            self.drive_state = State.STOPPED

        elif state == State.OUT_RAMP_UP:
            if self.tv_position != Position.OUT_END:
                self.direction_set(Direction.OUT)
                self.speed_set(self.SLOW_SPEED)
                self.drive_state = State.OUT_RAMP_UP
            else:
                self.drive_state = State.STOPPED

        elif state == State.OUT:
            self.speed_set(self.FAST_SPEED)
            self.drive_state = State.OUT

        elif state == State.OUT_RAMP_DOWN:
            self.drive_state = State.OUT_RAMP_DOWN

        elif state == State.IN_RAMP_UP:
            if self.tv_position != Position.IN_END:
                self.direction_set(Direction.IN)
                self.speed_set(self.SLOW_SPEED)
                self.drive_state = State.IN_RAMP_UP
            else:
                self.drive_state = State.STOPPED

        elif state == State.IN:
            self.speed_set(self.FAST_SPEED)
            self.drive_state = State.IN

        elif state == State.IN_RAMP_DOWN:
            self.drive_state = State.IN_RAMP_DOWN

        logger.info(f'set_state: requested state {state}, new state: {self.drive_state}')

    def run_state_machine(self):
        with self.lock:
            if self.drive_state == State.STOPPED:
                pass

            elif self.drive_state == State.OUT_RAMP_UP:
                speed = self.speed_get() + self.SPEED_RAMP_UP_STEP
                if speed >= self.FAST_SPEED:
                    self.set_state(State.OUT)
                else:
                    self.speed_set(speed)

            elif self.drive_state == State.OUT:
                if self.tv_position == Position.OUT_SLOW:
                    self.set_state(State.OUT_RAMP_DOWN)

            elif self.drive_state == State.OUT_RAMP_DOWN:
                if self.tv_position == Position.OUT_END:
                    self.set_state(State.STOPPED)

                speed = self.speed_get()
                if speed > self.SLOW_SPEED:
                    speed -= self.SPEED_RAMP_DOWN_STEP
                    if speed > self.SLOW_SPEED:
                        self.speed_set(speed)
                    else:
                        self.speed_set(self.SLOW_SPEED)

            elif self.drive_state == State.IN_RAMP_UP:
                speed = self.speed_get() + self.SPEED_RAMP_UP_STEP
                if speed >= self.FAST_SPEED:
                    self.set_state(State.IN)
                else:
                    self.speed_set(speed)

            elif self.drive_state == State.IN:
                if self.tv_position == Position.IN_SLOW:
                    self.set_state(State.IN_RAMP_DOWN)

            elif self.drive_state == State.IN_RAMP_DOWN:
                if self.tv_position == Position.IN_END:
                    self.set_state(State.STOPPED)

                speed = self.speed_get()
                if speed > self.SLOW_SPEED:
                    speed -= self.SPEED_RAMP_DOWN_STEP
                    if speed > self.SLOW_SPEED:
                        self.speed_set(speed)
                    else:
                        self.speed_set(self.SLOW_SPEED)

            # From any running state, go to stop if the end is reached
            if self.drive_state != State.STOPPED:
                if (((self.tv_position == Position.IN_END) and (self.motor_direction == Direction.IN) and (self.motor_speed > 0)) or
                   ((self.tv_position == Position.OUT_END) and (self.motor_direction == Direction.OUT) and (self.motor_speed > 0))):
                    self.set_state(State.STOPPED)

    def thread_process(self):
        self.thread_run = True
        logger.info('Thread process started')

        while (self.thread_run):
            self.run_state_machine()

            # All timing based on 100 ms timebase
            time.sleep(0.1)
