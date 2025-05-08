#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "ML/smart_grid_model_consumption.h"
#include "ML/smart_grid_model_solar.h"

#define PRICE_FACTOR 0.33  // Fattore per calcolare il prezzo di vendita (1/3 del prezzo di acquisto)
#define MAX_FEATURES 5     // Numero massimo di feature per i modelli

static struct etimer timer;
static int tempo_limite = 0;  // Tempo in ore entro il quale il task deve essere completato
static float consumo_corrente = 0.0;  // Consumo energetico corrente
static float produzione_corrente = 0.0;  // Produzione energetica corrente

// Funzione per calcolare il momento più efficiente per avviare il dispositivo
void calcola_momento_efficiente(float *features, int n_features, float prezzo_acquisto) {
    float consumo_predetto = smart_grid_model_consumption_regress1(features, n_features);
    float produzione_solare_predetta = smart_grid_model_solar_regress1(features, n_features);
    float prezzo_vendita = prezzo_acquisto * PRICE_FACTOR;

    printf("Consumo Predetto: %.2f kW\n", consumo_predetto);
    printf("Produzione Solare Predetta: %.2f kW\n", produzione_solare_predetta);
    printf("Prezzo di Vendita: %.2f USD/kWh\n", prezzo_vendita);

    if (produzione_solare_predetta > consumo_predetto) {
        printf("Momento efficiente trovato: Avvio del dispositivo.\n");
        avvia_dispositivo();
    } else {
        printf("Non è il momento più efficiente per avviare il dispositivo.\n");
    }
}

// Funzione per avviare il dispositivo
void avvia_dispositivo() {
    printf("Dispositivo avviato con successo!\n");
    // Aggiungi qui la logica per avviare il dispositivo (es. invio comando CoAP)
}

// Callback per gestire i messaggi CoAP ricevuti
void coap_message_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    if (len > 0) {
        char payload_str[128];
        snprintf(payload_str, sizeof(payload_str), "%.*s", (int)len, (char *)payload);
        printf("Messaggio CoAP ricevuto: %s\n", payload_str);

        // Parsing del payload per ottenere il tempo limite, il consumo e la produzione
        sscanf(payload_str, "{\"tempo_limite\": %d, \"consumo\": %f, \"produzione\": %f}", &tempo_limite, &consumo_corrente, &produzione_corrente);
        printf("Tempo limite ricevuto: %d ore\n", tempo_limite);
        printf("Consumo corrente: %.2f kW\n", consumo_corrente);
        printf("Produzione corrente: %.2f kW\n", produzione_corrente);
    }

    // Risposta al server
    coap_set_status_code(response, CHANGED_2_04);
    snprintf((char *)buffer, preferred_size, "Tempo limite impostato a %d ore, consumo %.2f kW, produzione %.2f kW", tempo_limite, consumo_corrente, produzione_corrente);
    coap_set_payload(response, buffer, strlen((char *)buffer));
}

// Processo principale
PROCESS(smartplug_process, "Smart Plug Process");
AUTOSTART_PROCESSES(&smartplug_process);

PROCESS_THREAD(smartplug_process, ev, data) {
    PROCESS_BEGIN();

    printf("Avvio del dispositivo IoT Smart Plug...\n");

    // Registra la risorsa CoAP per ricevere messaggi
    static coap_resource_t coap_resource;
    coap_activate_resource(&coap_resource, "gestione");
    coap_resource.handler = coap_message_handler;

    // Inizializza il timer per eseguire il calcolo periodicamente
    etimer_set(&timer, CLOCK_SECOND * 60);  // Esegui ogni 60 secondi

    while (1) {
        PROCESS_WAIT_EVENT();

        if (etimer_expired(&timer)) {
            // Simula le feature di input (sostituisci con dati reali)
            float features[MAX_FEATURES] = {1, 12, 230.0, 5.0, 0.15};  // Mese, ora, tensione, corrente, prezzo energia
            float prezzo_acquisto = features[4];  // Prezzo di acquisto dell'energia

            // Calcola il momento più efficiente
            if (tempo_limite > 0) {
                calcola_momento_efficiente(features, MAX_FEATURES, prezzo_acquisto);
            } else {
                printf("Nessun tempo limite impostato. In attesa di messaggi CoAP...\n");
            }

            // Reset del timer
            etimer_reset(&timer);
        }
    }

    PROCESS_END();
}