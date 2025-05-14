#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "coap-blocking-api.h"
#include "cJSON.h"

#include "sys/log.h"
#include "os/dev/leds.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"    // Esempio di endpoint

//funzionalit- minime
static struct etimer sleep_timer;
static char *tipo_dispositivo = "actuator";
static int max_registration_retry = 3;  // Numero massimo di tentativi di registrazione
static bool is_registered = false;  // Stato di registrazione
static int number_of_retries = 0;  // Numero di tentativi di registrazione

// Variabili globali per memorizzare i dati ricevuti
static int  tipo = 0;
static int  ora = 0;
static int  minuti = 0;
static int  giorno = 0;
static int  mese = 0;
static char solar_ip[64] = {0};
static char power_ip[64] = {0};
static bool orologio_attivo = false;  // Flag per l'orologio
static etimer_t clock_timer;  // Timer per l'orologio
static int stato_dispositivo = 0;  // Stato del dispositivo: 0=Spento, 1=Attivo, 2=Pronto

PROCESS(registra_dispositivo_process, "Registra Dispositivo Process");
PROCESS(smartplug_process, "Smart Plug Process");

AUTOSTART_PROCESSES(&smartplug_process);

static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    //handler per quando il server vuole mandare il sensore solar
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    cJSON *json = cJSON_Parse(payload);
    if (json == NULL) {
        LOG_ERR("Errore nel parsing del JSON\n");
        return;
    }

    cJSON *tipo = cJSON_GetObjectItem(json, "tipo");
    cJSON *ora = cJSON_GetObjectItem(json, "ora");
    cJSON *minuti = cJSON_GetObjectItem(json, "minuti");
    cJSON *giorno = cJSON_GetObjectItem(json, "giorno");
    cJSON *mese = cJSON_GetObjectItem(json, "mese");
    cJSON *solar_ip = cJSON_GetObjectItem(json, "solar_ip");
    cJSON *power_ip = cJSON_GetObjectItem(json, "power_ip");

    if (cJSON_IsNumber(tipo) && cJSON_IsNumber(ora) && cJSON_IsNumber(minuti) &&
        cJSON_IsNumber(giorno) && cJSON_IsNumber(mese) &&
        cJSON_IsString(solar_ip) && cJSON_IsString(power_ip)) {
        // Memorizza i dati nelle variabili globali
         tipo = tipo->valueint;
         ora = ora->valueint;
         minuti = minuti->valueint;
         giorno = giorno->valueint;
         mese = mese->valueint;
        strncpy( solar_ip, solar_ip->valuestring, sizeof( solar_ip) - 1);
        strncpy( power_ip, power_ip->valuestring, sizeof( power_ip) - 1);

        LOG_INFO("Registrazione ricevuta e memorizzata:\n");
        LOG_INFO("Tipo: %d\n",  tipo);
        LOG_INFO("Ora: %d:%d\n",  ora,  minuti);
        LOG_INFO("Data: %d/%d\n",  giorno,  mese);
        LOG_INFO("Solar IP: %s\n",  solar_ip);
        LOG_INFO("Power IP: %s\n",  power_ip);
        // Inizializza il timer per l'orologio
        etimer_set(&clock_timer, 60 * CLOCK_SECOND);  // Aggiorna ogni minuto
        orologio_attivo = true;  // Attiva l'orologio
    } else {
        LOG_ERR("JSON non valido o campi mancanti\n");
    }

    cJSON_Delete(json);
}
EVENT_RESOURCE(res_smartplug,
               "title=\"Smartplug resource\";actuator",
               NULL,
               res_post_handler,
               NULL,
               NULL,
               NULL);


void client_registration_handler(coap_message_t *response) {
    if (response == NULL) {
        LOG_ERR("Request timed out\n");
        is_registered = false;
        return;
    }

    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    if (len > 0) {
        registration_payload_handler((const char *)payload);
        max_registration_retry = 0;  // Registrazione riuscita, non tentare più
        LOG_INFO("Registrazione riuscita\n");
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
    //avvio risorsa 
    coap_activate_resource(&res_smartplug, "smartplug");
    while (1) {
        PROCESS_WAIT_EVENT();
        // Disattiva il dispositivo quando il task timer scade
        if (etimer_expired(&task_timer)) {
            disattiva_dispositivo();
        }
        if (etimer_expired(&clock_timer)) {
            aggiorna_orologio();
        }
    }
    PROCESS_END();
}
void disattiva_dispositivo() {
    process_start(&disattiva_dispositivo_process, NULL);
    
}

void aggiorna_orologio() {
    if (orologio_attivo) {
        minuti += 1;  // Incrementa i minuti di 1
        if (minuti >= 60) {
            minuti = 0;
            ore = (ore + 1) % 24;  // Incrementa le ore e gestisce il formato 24 ore
            if (ore == 0) {  // Incrementa il giorno a mezzanotte
                giorno += 1;
                if ((mese == 1 || mese == 3 || mese == 5 || mese == 7 || mese == 8 || mese == 10 || mese == 12) && giorno > 31) {
                    giorno = 1;
                    mese = (mese % 12) + 1;
                } else if ((mese == 4 || mese == 6 || mese == 9 || mese == 11) && giorno > 30) {
                    giorno = 1;
                    mese = (mese % 12) + 1;
                } else if (mese == 2 && giorno > 28) {  // Non gestiamo anni bisestili
                    giorno = 1;
                    mese = 3;
                }
            }
        }
        printf("Orologio aggiornato: %02d:%02d, Giorno: %02d, Mese: %02d\n", ore, minuti, giorno, mese);
        etimer_reset(&clock_timer);  // Reset del timer per il prossimo aggiornamento
    }
}



PROCESS_THREAD(registra_dispositivo_process, ev, data) {
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];

    PROCESS_BEGIN();

    LOG_INFO("Starting SmartPlug Server\n");
    // Attivo la risorsa CoAP

    while (max_registration_retry != 0) {
        /* -------------- REGISTRAZIONE --------------*/
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, "register/");
        const char msg[] = "{\"t\": \"actuator\",\"n"}";
        coap_set_payload(request, (uint8_t *)msg, strlen(msg));
        printf("Invio richiesta di registrazione: %s\n", msg);
        COAP_BLOCKING_REQUEST(&main_server_ep, request, client_registration_handler);

        /* -------------- END REGISTRAZIONE --------------*/
        if (!is_registered) {
            max_registration_retry--;
            printf("Registrazione non riuscita. Tentativi rimasti: %d\n", max_registration_retry);
            
            printf("Registrazione fallita. Attendo 30 secondi prima di riprovare.\n");
            etimer_set(&sleep_timer, 30 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
            
            
        }
    }

    if (is_registered) {
        LOG_INFO("REGISTRATION SUCCESS\n");
    }
    PROCESS_END();
}

PROCESS_THREAD(disattiva_dispositivo_process, ev, data) {
    PROCESS_BEGIN();

    static coap_endpoint_t server_endpoint;
    static coap_message_t request[1];

    const char *server_url = "coap://[fd00::1]:5683/device_status";  // URL del server

    // Imposta lo stato del dispositivo a 0 (Spento)
    stato_dispositivo = 0;
    printf("Dispositivo disattivato. Stato impostato a: %d (Spento)\n", stato_dispositivo);

    // Configura l'endpoint del server
    coap_endpoint_parse(server_url, strlen(server_url), &server_endpoint);
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "device_status");

    // Payload per notificare la disattivazione
    const char *payload = "{\"tipo\": \"3\", \"status\": \"inactive\"}";
    coap_set_payload(request, (uint8_t *)payload, strlen(payload));

    printf("Invio segnale al server: %s\n", payload);
    printf("Dimensione del payload: %zu\n", strlen(payload));

    // Invia la richiesta al server
    COAP_BLOCKING_REQUEST(&server_endpoint, request, client_chunk_handler);

    printf("Segnale di disattivazione inviato al server con successo.\n");

    PROCESS_END();
}
