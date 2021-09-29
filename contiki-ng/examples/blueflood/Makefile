SHELL:=/bin/sh

UART_BR_CONF?=UART_BAUDRATE_BAUDRATE_Baud115200 #UART bitrate
uartbr=115200

Nrf_1Mbit=0 #/*!< 1 Mbit/s Nordic proprietary radio mode */
Nrf_2Mbit=1 #/*!< 2 Mbit/s Nordic proprietary radio mode */
Ble_1Mbit=3 #/*!< 1 Mbit/s Bluetooth Low Energy */
Ble_2Mbit=4 #/*!< 2 Mbit/s Bluetooth Low Energy */
Ble_LR125Kbit=5 #/*!< Long range 125 kbit/s (TX Only - RX supports both) */
Ble_LR500Kbit=6 #/*!< Long range 500 kbit/s (TX Only - RX supports both) */
Ieee802154_250Kbit=15 #/*!< IEEE 802.15.4-2006 250 kbit/s */

CONTIKI = ../..
ifndef TARGET
TARGET=nrf52840
endif

CONTIKI_PROJECT = test-blueflood
PROJECT_SOURCEFILES += nrf-radio-driver.c blueflood.c
#PROJECT_CONF_PATH = project-conf.h
CFLAGS += -DUART_BAUDRATE=$(UART_BR_CONF)

ifdef TESTBED_CONF
CFLAGS += -DTESTBED=$(TESTBED_CONF)
endif

#save firmware with timestamp
DATE:=$(shell date +'%Y_%m_%d_%H_%M_%S')
FIRMWARE_NAME:=$(DATE)_$(CONTIKI_PROJECT)
FIRMWARE_FULL_PATH:=testbedhex/$(FIRMWARE_NAME).elf

all: $(CONTIKI_PROJECT)
	cp build/nrf52840/dk/$(CONTIKI_PROJECT).hex $(FIRMWARE_NAME).hex

include $(CONTIKI)/Makefile.include