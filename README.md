cems

is a suite of binaries to handle ems bus communication

emsSerio   is responsible for handling the communication to the EMS bus
	   via a tty interface

emsDecode  decodes the received messages

emsMqtt	   sends read values to a mqtt broker

emsMsb	   does the same to a MSB bus of a V4K installation

emsCommand shall send commands to the MMC110 or the RC310 of a Buderus system

emsMonitor show the same values from the shared memeory


architecture

emsSerio writes received data from the tty to a message queue and reads another message queue
for things to send to the tty.

emsDecode reads the messages from the receive queue and decodes it. The decoded
values are written to a shared memory segment.

emsMqtt and emsMsb read the values from this shared memory segment and write these
to a mqtt broker (server) resp. a MSB bus.



Prereq.

emsMqtt - please install libmosquitto-dev

emsMsb - please install research-virtualfortknox/msb-client-websocket-c from github
and follow the (scare) installation docs (you have to install several libs)


