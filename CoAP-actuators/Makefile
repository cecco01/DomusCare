CONTIKI_PROJECT = asciugatrice lavastoviglie lavatrice

all: $(CONTIKI_PROJECT)




CONTIKI = /home/iot_ubuntu_intel/contiki-ng
TARGET_LIBFILES += -lm
# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap
INC += /home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn
MODULES_REL += /home/iot_ubuntu_intel/.local/lib/python3.10/site-packages/emlearn

# Add the utils directory
MODULES_REL += ./resources/utils

LDLIBS += -lm

CFLAGS += -Wno-unused-variable -Wno-unused-function

include $(CONTIKI)/Makefile.include