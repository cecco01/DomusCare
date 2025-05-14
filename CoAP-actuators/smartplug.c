#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "coap-blocking-api.h"
#include "../ML/smart_grid_model_consumption.h"
#include "../ML/smart_grid_model_production.h"

#define MAX_FEATURES 5     // Numero massimo di feature per i modelli (senza il timestamp anno)
#define INTERVALLO_PREDIZIONE 900  // Intervallo di predizione in secondi (15 minuti)

const char *server_ep = "coap://[fd00::1]:5683";  // Esempio di endpoint

static char SOLAR_EP[64];  // Buffer per l'indirizzo IP del sensore solare
static char POWER_EP[64];  // Buffer per l'indirizzo IP del sensore di consumo
static struct etimer efficient_timer;  // Timer per avviare il dispositivo nel momento più efficiente
static struct etimer clock_timer;  // Timer per aggiornare l'orologio ogni minuto
static struct etimer task_timer;  // Timer per disattivare il dispositivo dopo la durata del task
static struct etimer sleep_timer;
static int tempo_limite = 0;  // Tempo in ore entro il quale il task deve essere completato
static char *nome_dispositivo = "Lavatrice";  // Nome del dispositivo
static float consumo = 0.0;  // Consumo energetico corrente
static float produzione = 0.0;  // Produzione energetica corrente
static int ore = 0;  // Ore per il task
static int minuti = 0;  // Minuti per il task
static int mese = 0;  // Mese corrente
static int giorno = 0;  // Giorno corrente
static int durata_task = 60 * 60;  // Durata del task in secondi
static float features_consumo[MAX_FEATURES];  // Array delle feature per il modello di consumo
static float features_produzione[MAX_FEATURES];  // Array delle feature per il modello di produzione
// Risorsa CoAP per lo stato del dispositivo
static int stato_dispositivo = 0;  // Stato del dispositivo: 0=Spento, 1=Attivo, 2=Pronto
static float consumo_dispositivo = 1.5;  // Consumo energetico corrente
static int numero_ripetizioni = 0;  // Dichiarazione globale
static int orologio_attivo = 0;  // Flag per indicare se l'orologio è attivo
static int durata = 60;

PROCESS(avvia_dispositivo_process, "Avvia Dispositivo Process");
PROCESS(richiedi_dati_sensore_process, "Richiedi Dati Sensore Process");
PROCESS(smartplug_process, "Smart Plug Process");
PROCESS(disattiva_dispositivo_process, "Disattiva Dispositivo Process");
PROCESS(registra_dispositivo_process, "Registra Dispositivo Process");

void calcola_momento_migliore();
void richiedi_dati_sensore(const char *server_ep, float *dato_ricevuto);
void avvia_dispositivo();
float model_consumption_regress1(const float *features, int num_features);
float model_production_regress1(const float *features, int num_features);
static bool is_registered = false;  // Flag per indicare se il dispositivo è registrato
static int number_of_retries = 0;  // Numero di tentativi di registrazione
void client_chunk_handler(coap_message_t *response) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(response, &payload);
    if (len > 0) {
        printf("Risposta ricevuta: %.*s\n", (int)len, (const char *)payload);
    } else {
        printf("Nessun payload ricevuto nella risposta.\n");
    }
}
void client_registration_handler(coap_message_t *response) {
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

static void stato_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer,
                          uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    if (len > 0) {
        int nuovo_stato;
        sscanf((const char *)payload, "%d", &nuovo_stato);

        if (nuovo_stato >= 0 && nuovo_stato <= 2) {
            stato_dispositivo = nuovo_stato;
            printf("Stato dispositivo aggiornato a: %d\n", stato_dispositivo);

            if (stato_dispositivo == 2) {
                printf("Dispositivo in modalità Pronto. Avvio gestione per l'efficienza energetica.\n");
                
                
                // Calcola il momento migliore per avviare il dispositivo
                calcola_momento_migliore();
            } else if (stato_dispositivo == 1 || stato_dispositivo == 0) {
                if (stato_dispositivo == 1) {
                    avvia_dispositivo();  // Avvia il dispositivo
                } 
                printf("Dispositivo in modalità %s. Interruzione della gestione.\n",
                       stato_dispositivo == 1 ? "Attivo" : "Spento");
                etimer_stop(&efficient_timer);  // Ferma la gestione
            }

            snprintf((char *)buffer, preferred_size, "Stato aggiornato a %d", stato_dispositivo);
            coap_set_payload(response, buffer, strlen((char *)buffer));
            coap_set_status_code(response, CHANGED_2_04);
        } else {
            snprintf((char *)buffer, preferred_size, "Stato non valido");
            coap_set_payload(response, buffer, strlen((char *)buffer));
            coap_set_status_code(response, BAD_REQUEST_4_00);
        }
    } else {
        snprintf((char *)buffer, preferred_size, "Stato corrente: %d", stato_dispositivo);
        coap_set_payload(response, buffer, strlen((char *)buffer));
        coap_set_status_code(response, CONTENT_2_05);
    }
}

// Funzione per richiedere nuovi dati al sensore CoAP
void richiedi_dati_sensore(const char *server_ep, float *dato_ricevuto) {
    process_start(&richiedi_dati_sensore_process, (void *)server_ep);
}

// Funzione per calcolare il momento migliore per avviare il dispositivo
void calcola_momento_migliore() {
    printf("Inizio calcolo del momento migliore per avviare il dispositivo...\n");

    richiedi_dati_sensore(SOLAR_EP, &produzione);
    richiedi_dati_sensore(POWER_EP, &consumo);
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

// Funzione per disattivare il dispositivo
void disattiva_dispositivo() {
    process_start(&disattiva_dispositivo_process, NULL);
    
}

// Funzione per avviare il dispositivo
void avvia_dispositivo() {
    process_start(&avvia_dispositivo_process, NULL);
}

// Funzione per registrare il dispositivo con il server
void registra_dispositivo(const char *server_url,  const char *tipo_dispositivo) {
    process_start(&registra_dispositivo_process, (void *)server_url);
    
}

// Funzione per aggiornare l'orologio
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

// Callback per gestire i messaggi CoAP ricevuti
void coap_message_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);

    if (len > 0) {
        char payload_str[256];
        snprintf(payload_str, sizeof(payload_str), "%.*s", (int)len, (char *)payload);
        printf("Messaggio CoAP ricevuto: %s\n", payload_str);

        // Parsing iniziale per il tipo di messaggio
        int tipo_messaggio = -1;
        sscanf(payload_str, "{\"tipo\": %d", &tipo_messaggio);

        if (tipo_messaggio == 0) {
            // Parsing per ora, minuti, giorno e mese
            sscanf(payload_str, "{\"tipo\": %d, \"ora\": %d, \"minuti\": %d, \"giorno\": %d, \"mese\": %d, \"solar_ip\": \"%63[^\"]\", \"power_ip\": \"%63[^\"]\"}",
                   &tipo_messaggio, &ore, &minuti, &giorno, &mese, SOLAR_EP, POWER_EP);
            printf("Tipo: %d, Ora: %02d, Minuti: %02d, Giorno: %02d, Mese: %02d, Solar IP: %s, Power IP: %s\n",
                   tipo_messaggio, ore, minuti, giorno, mese, SOLAR_EP, POWER_EP);

            etimer_set(&clock_timer, CLOCK_SECOND * 60);  // Imposta il timer per aggiornare ogni minuto
            printf("Orologio avviato.\n");

            coap_set_status_code(response, CHANGED_2_04);
            snprintf((char *)buffer, preferred_size, "Ora, minuti, giorno e mese ricevuti e orologio avviato: %02d:%02d, Giorno: %02d, Mese: %02d",
                     ore, minuti, giorno, mese);
            coap_set_payload(response, buffer, strlen((char *)buffer));
        } else if (tipo_messaggio == 2) {
            // Parsing per i dati di produzione solare
            sscanf(payload_str, "{\"tipo\": %d, \"solar_power\": %f}", &tipo_messaggio, &produzione);
            printf("Tipo: %d, Produzione solare ricevuta: %.2f kW\n", tipo_messaggio, produzione);

            // Risposta al server
            coap_set_status_code(response, CHANGED_2_04);// imposta il codice di stato della risposta a 2.04 Changed. Questo codice indica che la richiesta è stata elaborata con successo e che la risorsa è stata modificata. In pratica, quando un client invia una richiesta di modifica (ad esempio un PUT o un POST), il server può rispondere con CHANGED_2_04 per confermare che l'operazione è stata eseguita correttamente. 
            snprintf((char *)buffer, preferred_size, "Produzione solare aggiornata: %.2f kW", produzione);
            coap_set_payload(response, buffer, strlen((char *)buffer));
        } /*else if (tipo_messaggio == 3) {
            // Parsing per i dati di consumo energetico
            sscanf(payload_str, "{\"tipo\": %d, \"consumo\": %f}", &tipo_messaggio, &consumo);
            printf("Tipo: %d, Consumo energetico ricevuto: %.2f kW\n", tipo_messaggio, consumo);

            // Risposta al server
            coap_set_status_code(response, CHANGED_2_04);
            snprintf((char *)buffer, preferred_size, "Consumo energetico aggiornato: %.2f kW", consumo);
            coap_set_payload(response, buffer, strlen((char *)buffer));
        }*/ // LUCA NON DOVREBBE SERVIRE PERCHE C E NELLA GESTIONE DELLA RISORSA 
        else {
            printf("Tipo di messaggio non supportato o non valido: %d\n", tipo_messaggio);
            coap_set_status_code(response, BAD_REQUEST_4_00);
            snprintf((char *)buffer, preferred_size, "Tipo di messaggio non valido");
            coap_set_payload(response, buffer, strlen((char *)buffer));
        }
    }
}

// Processo principale
AUTOSTART_PROCESSES(&smartplug_process);

PROCESS_THREAD(smartplug_process, ev, data) {
    PROCESS_BEGIN();

    printf("Avvio del dispositivo IoT Smart Plug...\n");

    // Registra il dispositivo con il server
    const char *server_url = "coap://[fd00::1]:5683";
    const char *tipo_dispositivo = "actuator";
    registra_dispositivo(server_url, tipo_dispositivo);

    // Registra la risorsa CoAP per ricevere messaggi
    static coap_resource_t coap_resource;
    coap_activate_resource(&coap_resource, "gestione");
    coap_resource.get_handler = NULL;

    // Registra la risorsa CoAP per lo stato
    static coap_resource_t stato_resource;
    coap_activate_resource(&stato_resource, "stato");
    stato_resource.get_handler = NULL;
    stato_resource.post_handler = stato_handler;

    while (1) {
        PROCESS_WAIT_EVENT();

        // Disattiva il dispositivo quando il task timer scade
        if (etimer_expired(&task_timer)) {
            disattiva_dispositivo();
        }

        // Aggiorna l'orologio ogni minuto
        if (etimer_expired(&clock_timer)) {
            aggiorna_orologio();
        }
    }

    PROCESS_END();
}

PROCESS_THREAD(richiedi_dati_sensore_process, ev, data) {
    PROCESS_BEGIN();

    static coap_endpoint_t server_endpoint;
    static coap_message_t request[1];
    static float *dato_ricevuto;
    const char *server_ep;

    // Recupera i dati passati al processo
    server_ep = (const char *)data;
    dato_ricevuto = (float *)data;

    coap_endpoint_parse(server_ep, strlen(server_ep), &server_endpoint);
    coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
    coap_set_header_uri_path(request, server_ep);

    printf("Richiesta dati al sensore: %s\n", server_ep);

    COAP_BLOCKING_REQUEST(&server_endpoint, request, client_chunk_handler);

    // Estrai il valore dal payload e assegnalo a dato_ricevuto
    const uint8_t *payload = NULL;
    size_t len = coap_get_payload(request, &payload);
    if (len > 0) {
        sscanf((const char *)payload, "%f", dato_ricevuto);
        printf("Dati ricevuti dal sensore: %.2f\n", *dato_ricevuto);
    } else {
        printf("Errore nella ricezione dei dati dal sensore.\n");
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
    coap_set_header_uri_path(request, "device_status");

    // Payload per notificare l'avvio
    const char *payload = "{\"status\": \"active\"}";
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

PROCESS_THREAD(registra_dispositivo_process, ev, data) {
    PROCESS_BEGIN();

    static coap_endpoint_t server_endpoint;
    static coap_message_t request[1];
    const char *server_url = (const char *)data;

    // Configura l'endpoint del server
    coap_endpoint_parse(server_url, strlen(server_url), &server_endpoint);
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "register/");
    while (!is_registered) {
        char payload[256];
        //avvia il timer di 30 secondi
        
        
        printf("invio al indirizzo ip del server %s: {\"t\": \"actuator\", \"n\": %s , \"s\": %d, \"c\": %.2f, \"d\": %d } \n: ", server_url, nome_dispositivo, stato_dispositivo, consumo_dispositivo, durata);
        
        snprintf(payload, sizeof(payload), "{\"t\": \"actuator\", \"n\": %s , \"s\": %d, \"c\": %.2f, \"d\": %d }", nome_dispositivo, stato_dispositivo, consumo_dispositivo, durata);
        printf("Invio segnale al server: %s\n", payload);
        printf("Dimensione del payload: %zu\n", strlen(payload));
        // Imposta il payload nella richiesta
        coap_set_payload(request, (uint8_t *)payload, strlen(payload));
        // Invia la richiesta al server
        COAP_BLOCKING_REQUEST(&server_endpoint, request, client_registration_handler);
        if(!is_registered) {
            etimer_set(&sleep_timer, 30 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
        } 
    }
    // Crea il payload JSON per la registrazione
    printf("Registrazione completata con successo.\n");

    PROCESS_END();
}