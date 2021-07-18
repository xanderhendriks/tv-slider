TV Slider
=========

The TV Slider runs on a standard Raspbian image with the following files from this repo added:
* scripts/requirement.txt: Requirements for Python environment. Install: pip3 install -r scripts/requirement.txt
* scripts/tv_slider_flask_server.py, scripts/tv_slider_motor_control.py, scripts/tv_slider_mqtt.py: TV Slider application files
* scripts/templates/index.html: Web page that can be accessed on port 5000
* etc/systemd/system/tv-slider.service: Service definition file. Start with: sudo systemctl start tv-slider