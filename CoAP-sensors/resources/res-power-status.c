#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"

#include "power_status.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
//definisco una risorsa CoAP, GET per leggere lo stato, PUT per modificarlo(accendi/spegni)
RESOURCE(res_power_status,"title=\"Coap Power Status\";rt=\"Power_status\"",res_get_handler,NULL,res_put_handler,NULL);

//handler per la modifica dello stato (PUT)
static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    size_t len = 0;
    const char *value = NULL;
    int success = 1;

    if ((len = coap_get_post_variable(request, "value", &value))){
        if (strncmp(value, "0", len) == 0){
            status = 0;
        }
        else if (strncmp(value, "1", len) == 0){
            status = 1;
        }
        else{
            success = 0;
        }
    }
    else{
        success = 0;
    }

    if (success){
        coap_set_status_code(response, CHANGED_2_04);
        LOG_INFO("Power status changed to %d\n", status);
    }
    else{
        coap_set_status_code(response, BAD_REQUEST_4_00);
        LOG_ERR("Power status change failed\n");
    }
}

//handler per la lettura dello stato (GET): quando un client CoAP esegue GET, restituisce lo stato attuale sotto forma di JSON
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    coap_set_header_content_format(response, APPLICATION_JSON);
    int payload_len = snprintf((char *)buffer, preferred_size, "{\"sensor\":\"power\", \"status\":%d}", status);//cambia campo tipo del JSON VEDI solar.c
    coap_set_payload(response, buffer, payload_len);

    LOG_INFO("Payload: %s\n", buffer);
}