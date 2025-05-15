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
//static struct etimer task_timer;
static int max_registration_retry = 3;
static bool is_registered = false;
static int number_of_retries = 0;
//static int tipo = 0;
static int  minuti = 0, giorno = 0, mese = 0, ore = 0;
static char solar_ip[80] = {0};
static char power_ip[80] = {0};
static bool orologio_attivo = false;

static int stato_dispositivo = 0;  // Stato del dispositivo: 0=Spento, 1=Attivo, 2=Pronto
static int produzione = 0;  // Produzione energetica corrente
static int consumo = 0;  // Consumo energetico corrente
static int numero_ripetizioni = 0;  // Dichiarazione globale
static int tempo_limite = 0;  // Tempo in ore entro il quale il task deve essere completato
//variabili del dispositivo
/*
static char *nome_dispositivo = "Lavatrice";  // Nome del disposit
static float consumo_dispositivo = 1.5;  // Consumo energetico corrente
static int durata_task = 60 ;  // Durata del task in secondi
*/
PROCESS(registra_dispositivo_process, "Registra Dispositivo Process");
PROCESS(disattiva_dispositivo_process, "Disattiva Dispositivo Process");
PROCESS(smartplug_process, "Smart Plug Process");
PROCESS(richiedi_dati_sensore_process, "Richiedi Dati Sensore Process");

AUTOSTART_PROCESSES(&smartplug_process);

void disattiva_dispositivo(void);
void aggiorna_orologio(void);
void richiedi_dati_sensore(const char *server_ep, float *dato_ricevuto);
void client_chunk_handler(coap_message_t *response) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    
    if (len > 0) {
        printf("Risposta ricevuta: %.*s\n", (int)len, (const char *)payload);
        // ancora da gestire 
    } 
}

static char json[256];
static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);
    LOG_INFO("POST ricevuto\n");

    if (payload == NULL || len == 0) {
        coap_set_status_code(response, BAD_REQUEST_4_00);
        LOG_ERR("Payload non valido\n");
        return;
    }

    static size_t received_len = 0;

    // Accumula il payload nel buffer statico
    if (received_len + len <= sizeof(json)) {
        memcpy(json + received_len, payload, len);
        received_len += len;
        json[received_len] = '\0'; // Assicurati che sia null-terminated
    } else {
        LOG_ERR("Payload troppo grande, impossibile gestirlo\n");
        coap_set_status_code(response, BAD_REQUEST_4_00);
        return;
    }

    // Controlla se il payload contiene "/end/"
    if (strstr(json, "/end/") != NULL) {
        LOG_INFO("Payload completo ricevuto: %s\n", json);

        // Rimuovi "/end/" dal payload
        char *end_marker = strstr(json, "/end/");
        if (end_marker != NULL) {
            *end_marker = '\0'; // Termina la stringa prima di "/end/"
        }

        // Parsing manuale del JSON
        if (strstr(json, "\"o\":") != NULL) {
            sscanf(strstr(json, "\"o\":") + 5, "%d", &ore);
        }
        if (strstr(json, "\"m\":") != NULL) {
            sscanf(strstr(json, "\"m\":") + 5, "%d", &minuti);
        }
        if (strstr(json, "\"g\":") != NULL) {
            sscanf(strstr(json, "\"g\":") + 5, "%d", &giorno);
        }
        if (strstr(json, "\"h\":") != NULL) {
            sscanf(strstr(json, "\"h\":") + 5, "%d", &mese);
        }
        if (strstr(json, "\"s\":") != NULL) {
            sscanf(strstr(json, "\"s\":") + 6, "%79[^\"]", solar_ip);
        }
        if (strstr(json, "\"p\":") != NULL) {
            sscanf(strstr(json, "\"p\":") + 6, "%79[^\"]", power_ip);
        }

        LOG_INFO("Ora: %d:%d, Data: %d/%d\n", ore, minuti, giorno, mese);
        LOG_INFO("Solar IP: %s\n", solar_ip);
        LOG_INFO("Power IP: %s\n", power_ip);

        orologio_attivo = true;
        etimer_set(&clock_timer, 60 * CLOCK_SECOND);

        // Resetta il buffer per il prossimo payload
        memset(json, 0, sizeof(json));

        received_len = 0;
    }

    coap_set_status_code(response, CHANGED_2_04);
}
void richiedi_dati_sensore(const char *server_ep, float *dato_ricevuto) {
    process_start(&richiedi_dati_sensore_process, (void *)server_ep);
}

PROCESS_THREAD(richiedi_dati_sensore_process, ev, data) {
    PROCESS_BEGIN();

    static coap_endpoint_t server_endpoint;
    static coap_message_t request[1];
    float *dato_ricevuto = (float *)data;
    const char *server_ep = (const char *)data;

    coap_endpoint_parse(server_ep, strlen(server_ep), &server_endpoint);
    coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
    coap_set_header_uri_path(request, server_ep);

    printf("Richiesta dati al sensore: %s\n", server_ep);

    last_sensor_value_valid = 0;
    COAP_BLOCKING_REQUEST(&server_endpoint, request, client_chunk_handler);

    if (last_sensor_value_valid) {
        *dato_ricevuto = last_sensor_value;
        printf("Dati ricevuti dal sensore: %.2f\n", *dato_ricevuto);
    } else {
        printf("Errore nella ricezione dei dati dal sensore.\n");
    }

    PROCESS_END();
}

void calcola_momento_migliore() {
    printf("Inizio calcolo del momento migliore per avviare il dispositivo...\n");

    richiedi_dati_sensore(solar_ip, &produzione);
    richiedi_dati_sensore(power_ip, &consumo);
    features_produzione[0] = mese;  // Mese
    features_produzione[1] = giorno;  // Giorno
    features_produzione[2] = ore;  // Ora
    features_produzione[3] = produzione;  // Potenza solare (kW)
    features_consumo[0] = mese;     // Mese
    features_consumo[1] = giorno;   // Giorno
    features_consumo[2] = ore;      // Ora
    features_consumo[3] = consumo;    // Tensione (Volt)
    float consumo_predetto = model_consumption_regress1(features_produzione, MAX_FEATURES);
    float produzione_solare_predetta = model_production_regress1(features_produzione, MAX_FEATURES);

    // Controlla se c'è surplus energetico
    if (produzione_solare_predetta > consumo_predetto + consumo_dispositivo && produzione > consumo) {
        
        avvia_dispositivo();
    } else {
        printf("Non è stato possibile trovare un momento con surplus energetico entro la prossima ora.\n");
        printf("Richiedo nuovi dati dai sensori e riprovo tra 15 minuti.\n");
        numero_ripetizioni++;
        if(numero_ripetizioni== tempo_limite*4) {
            printf("Superato il numero massimo di tentativi. Avvio il dispositivo.\n");
            avvia_dispositivo();
            return;
        }
        // Riprova tra 15 minuti
        etimer_set(&efficient_timer, INTERVALLO_PREDIZIONE * CLOCK_SECOND);
    }
}
void calcola_momento_migliore();
void richiedi_dati_sensore(const char *server_ep, float *dato_ricevuto);
void avvia_dispositivo();
float model_consumption_regress1(const float *features, int num_features);
float model_production_regress1(const float *features, int num_features);
void avvia_dispositivo() {
    process_start(&avvia_dispositivo_process, NULL);
}
void remote_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    int nuovo_stato = 0;
    size_t len = coap_get_payload(request, &payload);
    LOG_INFO("POST remoto ricevuto\n");
    
    if (payload == NULL || len == 0) {
        coap_set_status_code(response, BAD_REQUEST_4_00);
        LOG_ERR("Payload non valido\n");
        return;
    }
    //parse il payload JSON
    char json[256];
    if (strstr(json, "\"s\":") != NULL) {
        sscanf(strstr(json, "\"s\":") + 5, "%d", &nuovo_stato);
    }
    if (strstr(json, "\"t\":") != NULL) {
        sscanf(strstr(json, "\"t\":") + 5, "%d", &tempo_limite);
    }
    if (nuovo_stato==2){
        calcola_momento_migliore();
    }
    if (nuovo_stato==0){
        disattiva_dispositivo();
    }
    if (nuovo_stato==1){
        avvia_dispositivo();
    }
    // Gestisci il payload remoto qui
    // Ad esempio, puoi inviare un messaggio al server o eseguire altre azioni

    coap_set_status_code(response, CHANGED_2_04);
}

EVENT_RESOURCE(res_smartplug,
               "title=\"Smartplug resource\";actuator",
               NULL,
               res_post_handler,
               NULL,
               NULL,
               NULL);
EVENT_RESOURCE(remote_smartplug,
               "title=\"remote_smartplug\";actuator",
               NULL,
               remote_post_handler,
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
        } else {
           // disattiva_dispositivo();
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
    if (len  >  0) { // Registrazione riuscita, non tentare più
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

    while (!is_registered) {
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

PROCESS_THREAD(avvia_dispositivo_process, ev, data) {
    PROCESS_BEGIN();

    static coap_endpoint_t server_endpoint;
    static coap_message_t request[1];

    const char *server_url = "coap://[fd00::1]:5683/device_status";  // URL del server

    // Imposta lo stato del dispositivo a 1 (Attivo)
    stato_dispositivo = 1;
    printf("Dispositivo avviato. Stato impostato a: %d (Attivo)\n", stato_dispositivo);

    // Configura l'endpoint del server
    coap_endpoint_parse(server_url, strlen(server_url), &server_endpoint);
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "Control/");

    // Payload per notificare l'avvio
    const char *payload = "{ \"t\":\"actuator\",\"stato\": \"1\",\"nome\": \"Lavatrice\"}";
    coap_set_payload(request, (uint8_t *)payload, strlen(payload));
    printf("Invio segnale al server: %s\n", payload);

    // Invia la richiesta al server
    COAP_BLOCKING_REQUEST(&server_endpoint, request, client_chunk_handler);

    printf("Segnale di avvio inviato al server con successo.\n");

    // Imposta il timer per disattivare il dispositivo dopo la durata del task
    etimer_set(&task_timer, durata_task * CLOCK_SECOND);
    printf("Timer impostato per disattivare il dispositivo dopo %d secondi.\n", durata_task);

    PROCESS_END();
}