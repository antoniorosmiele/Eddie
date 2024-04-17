FROM ubuntu:22.04
COPY run.sh /etc/init/
RUN apt update 
RUN DEBIAN_FRONTEND=noninteractive apt -y install \
    make cmake pkg-config libcoap3-dev build-essential gcovr \
    libgtest-dev dbus-x11 libglib2.0-dev doxygen openssh-client libjsoncpp-dev
RUN ["chmod", "+x", "/etc/init/run.sh"]
ENTRYPOINT ["/etc/init/run.sh"]