#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "coap-engine.h"
#include "utils/randomize.h"
#include "math.h"
#include "sys/log.h"
#include "coap-blocking-api.h"
#include <locale.h>

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"  // Indirizzo del server

#define MEAN 169.970005 // VALORI ANCORA DA STABILIRE
#define STDDEV 14.283898 // VALORI ANCORA DA STABILIRE

void res_power_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void handle(coap_message_t *response) {
    if (response == NULL) {
        LOG_ERR("Request timed out\n");
    } else {
        const uint8_t *payload = NULL;
        int len = coap_get_payload(response, &payload);
        if (len > 0) {
            printf("Response received: %.*s\n", len, (const char *)payload);
        } else {
            LOG_INFO("Dati inviati con successo\n");
        }
    }
}
RESOURCE(res_power,
         "title=\"valore\";rt=\"power\"",
         res_power_get_handler,  // Usa la funzione corretta
         NULL,
         NULL,
         NULL);

static double current_power = 0; // Memorizza l'ultimo valore generato

PROCESS(post_to_control_process, "Post to Control Process");

PROCESS_THREAD(post_to_control_process, ev, data) {

    static coap_endpoint_t control_server_ep;
    static coap_message_t request[1];

    PROCESS_BEGIN();
    current_power = generate_gaussian(MEAN, STDDEV);
    LOG_INFO("Power value: %.2f\n", current_power);
    // Imposta la locale numerica su "C"
    setlocale(LC_NUMERIC, "C");

    // Configura l'endpoint del server
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &control_server_ep);

    // Prepara il messaggio
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "control/");

    // Payload da inviare
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"t\": \"power\", \"value\": \"%.2f\"}", current_power);
    coap_set_payload(request, (uint8_t *)msg, strlen(msg));

    LOG_INFO("Invio POST alla risorsa control: %s\n", msg);

    // Invia la richiesta
    COAP_BLOCKING_REQUEST(&control_server_ep, request, handle);

    PROCESS_END();
}


void res_power_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    // Genera un payload JSON con il valore corrente di current_power
    int length = snprintf((char *)buffer, preferred_size, "{\"t\": \"power\", \"v\": \"%.2f\"}", current_power);
    LOG_INFO("GET RICEVUTA: %s\n", (char *)buffer);
    // Imposta il payload nella risposta
    coap_set_payload(response, buffer, length);

    LOG_INFO("Risposta GET inviata: %s\n", buffer);
}