FROM espressif/idf:release-v5.4

USER root
RUN apt-get update && apt-get install -y locales sudo udev && locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8

USER 1000

RUN echo "source $IDF_PATH/export.sh" > ~/.bashrc

ENTRYPOINT ["/bin/bash", "-c"]
SHELL ["/bin/bash", "-c"]
CMD ["bin/bash"]
