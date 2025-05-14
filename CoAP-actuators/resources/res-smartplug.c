#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"


/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

extern int stato_dispositivo;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);
static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    //handler per quando il server vuole mandare il sensore solar
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    if (len > 0) {
        int nuovo_stato;
        sscanf((const char *)payload, "%d", &nuovo_stato);
        if (nuovo_stato >= 0 && nuovo_stato <= 2) {
            stato_dispositivo = nuovo_stato;
            LOG_INFO("Stato dispositivo aggiornato a: %d\n", stato_dispositivo);
            res_event_handler();
        }
    }
}
EVENT_RESOURCE(res_smartplug,
               "title=\"Smartplug resource\";actuator",
               NULL,
               res_post_handler,
               NULL,
               NULL,
               res_event_handler);

static void
res_event_handler(void)
{
    LOG_INFO("Notifica osservatori: stato_dispositivo=%d\n", stato_dispositivo);
    coap_notify_observers(&res_smartplug);
}

static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    coap_set_header_content_format(response, APPLICATION_JSON);
    int payload_len = snprintf((char *)buffer, preferred_size, "{\"device\":\"smartplug\", \"status\":%d}", stato_dispositivo);
    coap_set_payload(response, buffer, payload_len);
}