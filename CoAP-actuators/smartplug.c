#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "ML/smart_grid_model_consumption.h"
#include "ML/smart_grid_model_solar.h"

#define PRICE_FACTOR 0.33  // Fattore per calcolare il prezzo di vendita (1/3 del prezzo di acquisto)
#define MAX_FEATURES 6     // Numero massimo di feature per i modelli (incluso il timestamp)
#define INTERVALLO_PREDIZIONE 900  // Intervallo di predizione in secondi (15 minuti)

static struct etimer efficient_timer;  // Timer per avviare il dispositivo nel momento più efficiente
static int tempo_limite = 0;  // Tempo in ore entro il quale il task deve essere completato
static float consumo_corrente = 0.0;  // Consumo energetico corrente
static float produzione_corrente = 0.0;  // Produzione energetica corrente
static int timestamp = 0;  // Timestamp ricevuto dal server

// Funzione per calcolare il momento migliore per avviare il dispositivo
void calcola_momento_migliore(float *features, int n_features, float prezzo_acquisto) {
    float miglior_costo = -1.0;
    int miglior_tempo = -1;

    printf("Inizio calcolo del momento migliore per avviare il dispositivo...\n");

    // Itera su tutti i possibili intervalli entro il tempo limite
    for (int i = 0; i <= tempo_limite * 4; i++) {  // Ogni iterazione rappresenta 15 minuti
        int tempo_attuale = timestamp + (i * INTERVALLO_PREDIZIONE);  // Calcola il timestamp futuro
        features[0] = tempo_attuale;  // Aggiorna il timestamp nelle feature

        // Predizioni
        float consumo_predetto = smart_grid_model_consumption_regress1(features, n_features);
        float produzione_solare_predetta = smart_grid_model_solar_regress1(features, n_features);

        // Calcola il costo netto
        float costo = (consumo_predetto - produzione_solare_predetta) * prezzo_acquisto;
        if (costo < 0) costo = 0;  // Evita costi negativi

        printf("Predizione per il tempo %d: Consumo %.2f kW, Produzione %.2f kW, Costo %.2f USD\n",
               tempo_attuale, consumo_predetto, produzione_solare_predetta, costo);

        // Aggiorna il momento migliore se il costo è inferiore
        if (miglior_costo == -1.0 || costo < miglior_costo) {
            miglior_costo = costo;
            miglior_tempo = i * INTERVALLO_PREDIZIONE;
        }
    }

    if (miglior_tempo >= 0) {
        printf("Momento migliore trovato tra %d secondi con costo %.2f USD. Avvio timer.\n",
               miglior_tempo, miglior_costo);
        etimer_set(&efficient_timer, miglior_tempo * CLOCK_SECOND);
    } else {
        printf("Non è stato possibile trovare un momento efficiente entro il tempo limite.\n");
    }
    miglior_costo = -1.0;  // Reset del miglior costo
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
        char payload_str[256];
        snprintf(payload_str, sizeof(payload_str), "%.*s", (int)len, (char *)payload);
        printf("Messaggio CoAP ricevuto: %s\n", payload_str);

        // Parsing del payload per ottenere il tempo limite, il consumo, la produzione e il timestamp
        sscanf(payload_str, "{\"tempo_limite\": %d, \"consumo\": %f, \"produzione\": %f, \"timestamp\": %d}", 
               &tempo_limite, &consumo_corrente, &produzione_corrente, &timestamp);
        printf("Tempo limite ricevuto: %d ore\n", tempo_limite);
        printf("Consumo corrente: %.2f kW\n", consumo_corrente);
        printf("Produzione corrente: %.2f kW\n", produzione_corrente);
        printf("Timestamp ricevuto: %d\n", timestamp);

        // Simula le feature di input (sostituisci con dati reali)
        float features[MAX_FEATURES] = {timestamp, 12, 230.0, consumo_corrente, produzione_corrente};  // Timestamp, ora, tensione, corrente, produzione
        float prezzo_acquisto = 0.15;  // Prezzo di acquisto dell'energia

        // Calcola il momento migliore
        calcola_momento_migliore(features, MAX_FEATURES, prezzo_acquisto);
    }

    // Risposta al server
    coap_set_status_code(response, CHANGED_2_04);
    snprintf((char *)buffer, preferred_size, "Tempo limite impostato a %d ore, consumo %.2f kW, produzione %.2f kW, timestamp %d", 
             tempo_limite, consumo_corrente, produzione_corrente, timestamp);
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

    while (1) {
        PROCESS_WAIT_EVENT();

        if (etimer_expired(&efficient_timer)) {
            printf("Timer scaduto: Avvio del dispositivo nel momento più efficiente.\n");
            avvia_dispositivo();
        }
    }

    PROCESS_END();
}