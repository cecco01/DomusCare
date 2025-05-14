#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"
#include "utils/randomize.h"
#include "math.h"
//Questo file implementa una risorsa CoAP osservabile e notifica periodicamente i client che si iscrivono

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"  // Indirizzo del server

#define MEAN 169.970005 //VALORI ANCORA DA STABILIRE ma sono valori realisitici per il Voltaggio
#define STDDEV 14.283898 //VALORI ANCORA DA STABILIRE

void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

//macro che definisce una risorsa osservabile CoAP
EVENT_RESOURCE(res_SolarPw,"title=\"Observable resource\";SolarPower",res_get_handler,NULL,NULL,NULL,res_event_handler);

static double current_solarpower = 0;//memorizza l'ultimo valore generato

//simula un nuovo valore e notifica tutti i CoAP client che stanno osservando la risorsa
static void res_event_handler(void){
  current_solarpower = generate_gaussian(MEAN, STDDEV);
  LOG_INFO("Payload to be sent: {\"t\":\"solar\",\"value\" : %.2f}\n", current_solarpower);//cambia campo tipo del JSON
  
    static coap_endpoint_t control_server_ep;
    static coap_message_t request[1];

    // Configura l'endpoint del server
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &control_server_ep);

    // Prepara il messaggio
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "control");

    // Payload da inviare
    const char msg[] = "{\"t\": \"solar\", \"status\": \"triggered\"}";
    coap_set_payload(request, (uint8_t *)msg, strlen(msg));

    // Invia la richiesta
    LOG_INFO("Invio POST alla risorsa control: %s\n", msg);
    COAP_BLOCKING_REQUEST(&control_server_ep, request, client_chunk_handler);
}

//quando un client CoAP richiede la risorsa, viene generato un payload JSON con il valore corrente
void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  int payload_len = snprintf((char *)buffer, preferred_size, "{\"solar\":%.2f}", current_solarpower);//cambia campo tipo del JSON
  coap_set_payload(response, buffer, payload_len);

  LOG_INFO("Payload: %s\n", buffer);
}