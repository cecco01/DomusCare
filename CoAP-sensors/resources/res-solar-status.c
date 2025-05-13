#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"

// permette di leggere e modificare lo stato del sensore di tensione (es. acceso/spento). Molto utile per controllo remoto degli attuatori o abilitazione/disabilitazione delle misurazioni

#include "solar_status.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
//definiamo una risorsa CoAP, GET per leggere lo stato, PUT per modificarlo(accendi/spegni)
RESOURCE(res_SolarPw_status,"title=\"Coap SolarPower Status\";rt=\"SolarPower_status\"",res_get_handler,NULL,res_put_handler,NULL);

//handler per la modifica dello stato (PUT)
static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    size_t len = 0;
    const char *value = NULL;
    int success = 1;

    if ((len = coap_get_post_variable(request, "value", &value))){//estraggo il parametro "value" dal corpo della richiesta
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

    if (success){//se la richiesta è andata a buon fine
        coap_set_status_code(response, CHANGED_2_04);
        LOG_INFO("SolarPower status changed to %d\n", status);
    }
    else{//se la richiesta non è andata a buon fine
        coap_set_status_code(response, BAD_REQUEST_4_00);
        LOG_ERR("SolarPower status change failed\n");
    }
}

//handler per la lettura dello stato (GET): quando un client CoAP esegue GET, restituisce lo stato attuale sotto forma di JSON
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    coap_set_header_content_format(response, APPLICATION_JSON);
    int payload_len = snprintf((char *)buffer, preferred_size, "{\"sensor\":\"SolarPower\", \"status\":%d}", status);//cambia campo tipo del JSON
    coap_set_payload(response, buffer, payload_len);

    LOG_INFO("Payload: %s\n", buffer);
}