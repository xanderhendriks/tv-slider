import logging

from flask_sse import sse


class RedisLoggingHandler(logging.Handler):
    def __init__(self, app):
        super().__init__()
        self.app = app

    def emit(self, record):
        log_entry = {
            'timestamp': record.created,
            'level': record.levelname,
            'message': record.getMessage(),
            'module': record.module,
            'function': record.funcName,
            'line': record.lineno,
            'process': record.process,
            'thread': record.thread,
        }

        # Send to UI
        with self.app.app_context():
            sse.publish({'message': f"{log_entry['timestamp']} {log_entry['level']} {log_entry['message']}"}, type='message')
