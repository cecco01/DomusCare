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

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

EVENT_RESOURCE(res_power,"title=\"Observable resource\";power",res_get_handler,NULL,NULL,NULL,res_event_handler);

static double current_power = 0;//memorizza l'ultimo valore generato

//simula un nuovo valore e notifica tutti i CoAP client che stanno osservando la risorsa
static void res_event_handler(void){
  current_power = generate_gaussian(MEAN, STDDEV);
  LOG_INFO("Payload to be sent: {\"sensor\":\"power\", \"value\":%.2f}\n", current_power);
  coap_notify_observers(&res_power);
}

//quando un client CoAP richiede la risorsa, viene generato un payload JSON con il valore corrente
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  int payload_len = snprintf((char *)buffer, preferred_size, "{\"sensor\":\"power\", \"value\":%.2f}", current_power);
  coap_set_payload(response, buffer, payload_len);

  LOG_INFO("Payload: %s\n", buffer);
}