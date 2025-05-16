#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"
#include "utils/randomize.h"
#include "math.h"
#include "sys/log.h"
#include "coap-blocking-api.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"  // Indirizzo del server

#define MEAN 169.970005 // VALORI ANCORA DA STABILIRE
#define STDDEV 14.283898 // VALORI ANCORA DA STABILIRE

void res_solar_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_solar,
  "title=\"Produzione Energeticq\";rt=\"Text\"",
  res_get_handler,
  NULL,
  NULL,
  NULL);

static double current_solarpower = 0; // Memorizza l'ultimo valore generato

// Quando un client CoAP richiede la risorsa, viene generato un payload JSON con il valore corrente

PROCESS(post_to_solar_process, "Post to Solar Process");

PROCESS_THREAD(post_to_solar_process, ev, data) {
    static coap_endpoint_t control_server_ep;
    static coap_message_t request[1];

    PROCESS_BEGIN();

    // Configura l'endpoint del server
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &control_server_ep);

    // Prepara il messaggio
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "control/");

    // Payload da inviare
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"t\": \"solar\", \"value\": %.2f}", current_solarpower);
    coap_set_payload(request, (uint8_t *)msg, strlen(msg));

    LOG_INFO("Invio POST alla risorsa control: %s\n", msg);

    // Invia la richiesta
    COAP_BLOCKING_REQUEST(&control_server_ep, request, NULL);

    PROCESS_END();
}


void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    current_solarpower = generate_gaussian(MEAN, STDDEV);
    LOG_INFO("Power value: %.2f\n", current_solarpower);
    process_start(&post_to_solar_process, NULL);
}