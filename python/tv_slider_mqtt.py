import logging
import paho.mqtt.client as mqtt


logger = logging.getLogger(__name__)


class TvSliderMqtt:
    MQTT_SERVER = '192.168.0.253'
    MQTT_PORT = 1883

    def __init__(self, callback=None):
        # Instantiate the MQTT client
        self.mqtt_ignore_message = True
        self.mqtt_client = mqtt.Client(__name__)
        self.mqtt_client.connect(self.MQTT_SERVER, self.MQTT_PORT)

        self.mqtt_client.on_connect = self.mqtt_on_connect_callback

        # Create a MQTT subscriber thread to listen for tv-slider commands
        self.mqtt_client.on_message = self.mqtt_subscribe_callback

        self.callback = callback
        self.mqtt_client.loop_start()
        logger.info('Thread process started')

    def mqtt_on_connect_callback(self, client, userdata, flags, rc):
        logger.info(f'mqtt_on_connect_callback: rc: {rc}')
        if rc == 0:
            self.mqtt_client.subscribe('tv-slider/switch')

    def mqtt_subscribe_callback(self, client, userdata, message):
        if self.mqtt_ignore_message:
            self.mqtt_ignore_message = False
        else:
            logger.info(f'mqtt_subscribe_callback: topic: {message.topic}, payload: {message.payload}')
            self.mqtt_client.publish('tv-slider/state', message.payload.decode(), 0, True)

            if message.payload.decode() == 'ON':
                direction = 'OUT'
            else:
                direction = 'IN'

            if self.callback is not None:
                self.callback(direction)

    def end(self):
        self.mqtt_client.loop_stop()
        logger.info('Thread process stopped')
