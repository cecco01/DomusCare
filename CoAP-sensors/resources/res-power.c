#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"
#include "utils/randomize.h"
#include "math.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define MEAN 169.970005 //VALORI ANCORA DA STABILIRE
#define STDDEV 14.283898 //VALORI ANCORA DA STABILIRE

void res_power_get_handler(coap_message_t *request, coap_message_t *response,
                                  uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

EVENT_RESOURCE(res_power,"title=\"Observable resource\";power",res_power_get_handler,NULL,NULL,NULL,res_event_handler);

static double current_power = 0;//memorizza l'ultimo valore generato

//simula un nuovo valore e notifica tutti i CoAP client che stanno osservando la risorsa
static void res_event_handler(void){
  current_power = generate_gaussian(MEAN, STDDEV);
  LOG_INFO("Payload to be sent: {\"t\":\"power\", \"value\":%.2f}\n", current_power);//cambia campo tipo del JSON
  static void send_post_to_control() {
    static coap_endpoint_t control_server_ep;
    static coap_message_t request[1];

    // Configura l'endpoint del server
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &control_server_ep);

    // Prepara il messaggio
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "control");

    // Payload da inviare
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"tipo\": \"power\", \"valore\": %.2f}", current_power);
    coap_set_payload(request, (uint8_t *)msg, strlen(msg));

    // Invia la richiesta
    LOG_INFO("Invio POST alla risorsa control: %s\n", msg);
    COAP_BLOCKING_REQUEST(&control_server_ep, request, NULL);
}
}

//quando un client CoAP richiede la risorsa, viene generato un payload JSON con il valore corrente
 void res_power_get_handler(coap_message_t *request, coap_message_t *response,
                                  uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    coap_set_header_content_format(response, APPLICATION_JSON);
    int payload_len = snprintf((char *)buffer, preferred_size, "{\"power\":%.2f}", current_power);
    coap_set_payload(response, buffer, payload_len);

    LOG_INFO("Payload: %s\n", buffer);
}