#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "coap-blocking-api.h"

#include "sys/log.h"
#include "os/dev/leds.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define SERVER_EP "coap://[fd00::1]:5683"    // Esempio di endpoint

// Variabili globali
static struct etimer sleep_timer;
static struct etimer clock_timer;
static struct etimer task_timer;
static int max_registration_retry = 3;
static bool is_registered = false;
static int number_of_retries = 0;
static int tipo = 0, ora = 0, minuti = 0, giorno = 0, mese = 0, ore = 0;
static char solar_ip[64] = {0};
static char power_ip[64] = {0};
static bool orologio_attivo = false;
static int stato_dispositivo = 0;  // Stato del dispositivo: 0=Spento, 1=Attivo, 2=Pronto
//variabili del dispositivo
/*
static char *nome_dispositivo = "Lavatrice";  // Nome del disposit
static float consumo_dispositivo = 1.5;  // Consumo energetico corrente
static int durata_task = 60 ;  // Durata del task in secondi
*/

PROCESS(registra_dispositivo_process, "Registra Dispositivo Process");
PROCESS(disattiva_dispositivo_process, "Disattiva Dispositivo Process");
PROCESS(smartplug_process, "Smart Plug Process");

AUTOSTART_PROCESSES(&smartplug_process);

void disattiva_dispositivo(void);
void aggiorna_orologio(void);


void client_chunk_handler(coap_message_t *response) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    
    if (len > 0) {
        printf("Risposta ricevuta: %.*s\n", (int)len, (const char *)payload);
        // ancora da gestire 
    } 
}
static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    if (payload == NULL || len == 0) {
        LOG_ERR("Payload vuoto o nullo\n");
        return;
    }

    // Converte il payload in una stringa null-terminated
    char json[len + 1];
    memcpy(json, payload, len);
    json[len] = '\0';

    // Parsing manuale del JSON
    if (strstr(json, "\"ora\":") != NULL) {
        sscanf(strstr(json, "\"ora\":") + 6, "%d", &ora);
    }
    if (strstr(json, "\"minuti\":") != NULL) {
        sscanf(strstr(json, "\"minuti\":") + 9, "%d", &minuti);
    }
    if (strstr(json, "\"giorno\":") != NULL) {
        sscanf(strstr(json, "\"giorno\":") + 9, "%d", &giorno);
    }
    if (strstr(json, "\"mese\":") != NULL) {
        sscanf(strstr(json, "\"mese\":") + 7, "%d", &mese);
    }
    if (strstr(json, "\"solar_ip\":") != NULL) {
        sscanf(strstr(json, "\"solar_ip\":") + 12, "%63[^\"]", solar_ip);
    }
    if (strstr(json, "\"power_ip\":") != NULL) {
        sscanf(strstr(json, "\"power_ip\":") + 12, "%63[^\"]", power_ip);
    }

    // Verifica se tutti i campi sono stati estratti correttamente
    if (tipo >= 0 && ora >= 0 && minuti >= 0 && giorno > 0 && mese > 0 &&
        strlen(solar_ip) > 0 && strlen(power_ip) > 0) {
        LOG_INFO("Registrazione ricevuta e memorizzata:\n");
        LOG_INFO("Tipo: %d\n", tipo);
        LOG_INFO("Ora: %d:%d\n", ora, minuti);
        LOG_INFO("Data: %d/%d\n", giorno, mese);
        LOG_INFO("Solar IP: %s\n", solar_ip);
        LOG_INFO("Power IP: %s\n", power_ip);

        // Inizializza il timer per l'orologio
        etimer_set(&clock_timer, 60 * CLOCK_SECOND);  // Aggiorna ogni minuto
        orologio_attivo = true;  // Attiva l'orologio
    } else {
        LOG_ERR("JSON non valido o campi mancanti\n");
    }
}

EVENT_RESOURCE(res_smartplug,
               "title=\"Smartplug resource\";actuator",
               NULL,
               res_post_handler,
               NULL,
               NULL,
               NULL);

PROCESS_THREAD(smartplug_process, ev, data) {
    PROCESS_BEGIN();

    LOG_INFO("Avvio del dispositivo IoT Smart Plug...\n");

    // Registra il dispositivo con il server
    process_start(&registra_dispositivo_process, NULL);

    // Avvio risorsa
    coap_activate_resource(&res_smartplug, "smartplug");
    
    while (1) {
        PROCESS_WAIT_EVENT();
        
        if (etimer_expired(&clock_timer)) {
            aggiorna_orologio();
        }
        else{
            if (etimer_expired(&task_timer)) {
                disattiva_dispositivo();
            }
        }
    }
    PROCESS_END();
    
}
void disattiva_dispositivo(void) {
    process_start(&disattiva_dispositivo_process, NULL);
}

void aggiorna_orologio(void) {
    if (orologio_attivo) {
        minuti += 1;
        if (minuti >= 60) {
            minuti = 0;
            ore = (ore + 1) % 24;
            if (ore == 0) {
                giorno += 1;
                if ((mese == 1 || mese == 3 || mese == 5 || mese == 7 || mese == 8 || mese == 10 || mese == 12) && giorno > 31) {
                    giorno = 1;
                    mese = (mese % 12) + 1;
                } else if ((mese == 4 || mese == 6 || mese == 9 || mese == 11) && giorno > 30) {
                    giorno = 1;
                    mese = (mese % 12) + 1;
                } else if (mese == 2 && giorno > 28) {
                    giorno = 1;
                    mese = 3;
                }
            }
        }
        LOG_INFO("Orologio aggiornato: %02d:%02d, Giorno: %02d, Mese: %02d\n", ore, minuti, giorno, mese);
        etimer_reset(&clock_timer);
    }
}

void client_registration_handler(coap_message_t *response) {
    if (response == NULL) {
        LOG_ERR("Request timed out\n");
        is_registered = false;
        number_of_retries++;
        return;
    }
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    if (len  !=  0) {
        max_registration_retry = 0;  // Registrazione riuscita, non tentare più
        LOG_INFO("Registrazione non riuscita\n");
        is_registered = false;

    } else {
        is_registered = true ;
        number_of_retries++;
        LOG_INFO("Registrazione riuscita\n");

    }
}

PROCESS_THREAD(registra_dispositivo_process, ev, data) {
    static coap_endpoint_t main_server_ep;
    static coap_message_t request[1];

    PROCESS_BEGIN();

    LOG_INFO("Starting SmartPlug Server\n");

    while (max_registration_retry != 0) {
        /* -------------- REGISTRAZIONE --------------*/
        coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, "register/");
        char msg[256];
        // Inizializza il messaggio JSON
        snprintf(msg, sizeof(msg), "{\"t\": \"actuator\", \"n\": \"Lavatrice\", \"c\": \"1.5\", \"d\": \"60\"}");
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
    const char *payload = "{\"status\": \"off\"}";
    coap_set_payload(request, (uint8_t *)payload, strlen(payload));

    printf("Invio segnale al server: %s\n", payload);
    printf("Dimensione del payload: %zu\n", strlen(payload));

    // Invia la richiesta al server
    COAP_BLOCKING_REQUEST(&server_endpoint, request, client_chunk_handler);

    printf("Segnale di disattivazione inviato al server con successo.\n");

    PROCESS_END();
}
