#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "coap-blocking-api.h"
#include "../ML/smart_grid_model_consumption.h"
#include "../ML/smart_grid_model_production.h"
#include "sys/log.h"
#include "os/dev/leds.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"  // Esempio di endpoint

//funzionalit- minime
static struct etimer sleep_timer;
static char *nome_dispositivo = "Lavatrice";
static char *tipo_dispositivo = "actuator";
static int ore = 0;  // Ore per il task
static int minuti = 0;  // Minuti per il task
static int mese = 0;  // Mese corrente
static int giorno = 0;  // Giorno corrente
static int durata_task = 60 * 60;  // Durata del task in secondi
int stato_dispositivo = 0;  // Stato del dispositivo: 0=Spento, 1=Attivo, 2=Pronto
static float consumo_dispositivo = 1.5;  // Consumo energetico corrente
static int numero_ripetizioni = 0;  // Dichiarazione globale
static int orologio_attivo = 0;  // Flag per indicare se l'orologio è attivo
static int durata = 60;
static int max_registration_retry = 3;  // Numero massimo di tentativi di registrazione
static bool is_registered = false;  // Stato di registrazione
static int number_of_retries = 0;  // Numero di tentativi di registrazione

PROCESS(registra_dispositivo_process, "Registra Dispositivo Process");
PROCESS(smartplug_process, "Smart Plug Process");

AUTOSTART_PROCESSES(&smartplug_process);

void stato_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    // Implementazione del gestore POST
}

void client_registration_handler(coap_message_t *response) {
    if (response == NULL) {
        printf("Request timed out\n");
        is_registered = false;
        return;
    }

    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    if (len > 0) {
        is_registered = true;
        printf("Registrazione riuscita: %.*s\n", (int)len, (const char *)payload);
    } else {
        is_registered = false;
        number_of_retries++;
        printf("Registrazione non riuscita. Tentativo %d \n", number_of_retries);
    }
}

void registra_dispositivo(const char *tipo_dispositivo) {
    process_start(&registra_dispositivo_process, NULL);
}

PROCESS_THREAD(smartplug_process, ev, data) {
    PROCESS_BEGIN();

    printf("Avvio del dispositivo IoT Smart Plug...\n");

    // Registra il dispositivo con il server
    registra_dispositivo(tipo_dispositivo);

    // Registra la risorsa CoAP per lo stato
    static coap_resource_t stato_resource;
    coap_activate_resource(&stato_resource, "stato");
    stato_resource.get_handler = NULL;
    stato_resource.post_handler = stato_handler;

    PROCESS_END();
}

PROCESS_THREAD(registra_dispositivo_process, ev, data) {
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];

    PROCESS_BEGIN();

    LOG_INFO("Starting SmartPlug Server\n");
    // Attivo la risorsa CoAP
    static coap_resource_t res_smartplug;
    coap_activate_resource(&res_smartplug, "smartplug");

    while (max_registration_retry != 0) {
        /* -------------- REGISTRAZIONE --------------*/
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, "register/");
        const char msg[] = "{\"t\": \"solar\"}";
        coap_set_payload(request, (uint8_t *)msg, strlen(msg));

        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_registration_handler);

        /* -------------- END REGISTRAZIONE --------------*/
        if (!is_registered) {
            max_registration_retry--;
            printf("Registrazione non riuscita. Tentativi rimasti: %d\n", max_registration_retry);
            if (max_registration_retry == 0) {
                printf("Registrazione fallita. Attendo 30 secondi prima di riprovare.\n");
                etimer_set(&sleep_timer, 30 * CLOCK_SECOND);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
                max_registration_retry = 3;
            }
        }
    }

    if (is_registered) {
        LOG_INFO("REGISTRATION SUCCESS\n");
    }
    PROCESS_END();
}