CONTIKI_PROJECT = smartplug

all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
# PLATFORMS_EXCLUDE = sky z1

#Da terminare...
CONTIKI = /home/iot_ubuntu_intel/contiki-ng
MODULES += os/net/app-layer/coap
MODULES += os/services/orchestra
TARGET_LIBFILES += -lm
INC += /home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn
MODULES_REL += /home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn
MODULES_REL += ./resources
MAKE_MAC = MAKE_MAC_TSCH
CFLAGS += -Wno-error
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE
include $(CONTIKI)/Makefile.include
