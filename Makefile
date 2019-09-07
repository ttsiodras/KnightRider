MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR := $(patsubst %/,%,$(dir ${MKFILE_PATH}))
BUILD_DIR := ${CURRENT_DIR}/tmp

SRC:=KnightRider.ino
ELF:=tmp/${SRC}.elf

BASE:=/usr/share/arduino
USER_BASE:=$(HOME)/.arduino15
USER_LIBS:=$(HOME)/Arduino/libraries
BOARD:=arduino:avr:pro:cpu=8MHzatmega328

HARDWARE:=-hardware ${BASE}/hardware -hardware ${USER_BASE}/packages 
TOOLS:=-tools ${BASE}/tools-builder -tools ${USER_BASE}/packages
LIBRARIES=-built-in-libraries ${BASE}/lib
LIBRARIES+=-libraries ${USER_LIBS}  # Where U8g2 comes from
WARNINGS:=-warnings all -logger human

ARDUINO_BUILDER_OPTS=${HARDWARE} ${TOOLS} ${LIBRARIES}
ARDUINO_BUILDER_OPTS+=-fqbn=${BOARD} ${WARNINGS}
ARDUINO_BUILDER_OPTS+=-verbose -build-path ${BUILD_DIR} 

all:
	@mkdir -p ${BUILD_DIR}
	arduino-builder -compile ${ARDUINO_BUILDER_OPTS} ${SRC} >build.log 2>&1

tags:
	ctags -R . ${USER_LIBS} ${USER_BASE}

clean:
	rm -rf ${BUILD_DIR} build.log

upload:	all
	avrdude -pm328p -carduino -P /dev/ttyUSB0 -b57600 -U flash:w:${ELF}
	avr-size ${ELF}
