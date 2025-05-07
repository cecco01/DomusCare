#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "contiki.h"
#include "contiki-net.h"
#include "sys/etimer.h"
#include "DT_model.h"
#include "json.h"

#define DB_HOST "localhost"
#define DB_USER "Luca"
#define DB_PASS "Luca"
#define DB_NAME "iot"

// Funzione per verificare se un dispositivo esiste nel database
int dispositivo_esiste(MYSQL *conn, const char *nome) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM dispositivi WHERE nome = '%s'", nome);

    if (mysql_query(conn, query)) {
        printf("Errore nella query: %s\n", mysql_error(conn));
        return 0;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        printf("Errore nel recupero dei risultati: %s\n", mysql_error(conn));
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int count = atoi(row[0]);
    mysql_free_result(res);

    return count > 0;
}

void mostra_dispositivi(MYSQL *conn) {
    MYSQL_RES *res;
    MYSQL_ROW row;

    if (mysql_query(conn, "SELECT nome, tipo, stato FROM dispositivi")) {
        printf("Errore nella query: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        printf("Errore nel recupero dei risultati: %s\n", mysql_error(conn));
        return;
    }

    printf("Dispositivi:\n");
    while ((row = mysql_fetch_row(res))) {
        printf("Nome: %s, Tipo: %s, Stato: %s\n", row[0], row[1], atoi(row[2]) ? "Attivo" : "Inattivo");
    }

    mysql_free_result(res);
}

void cambia_stato_dispositivo(MYSQL *conn, const char *nome, int nuovo_stato, int ore) {
    if (!dispositivo_esiste(conn, nome)) {
        printf("Errore: Il dispositivo '%s' non esiste nel database.\n", nome);
        return;
    }

    // Verifica lo stato attuale del dispositivo
    char query[256];
    snprintf(query, sizeof(query), "SELECT stato FROM dispositivi WHERE nome = '%s'", nome);

    if (mysql_query(conn, query)) {
        printf("Errore nella query per ottenere lo stato: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        printf("Errore nel recupero dei risultati: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int stato_attuale = atoi(row[0]);
    mysql_free_result(res);

    // Cambia stato solo se lo stato attuale è 0
    if (stato_attuale != 0) {
        printf("Errore: Il dispositivo '%s' non è inattivo (stato attuale: %d).\n", nome, stato_attuale);
        return;
    }

    // Aggiorna lo stato del dispositivo e il timestamp di attivazione
    if (nuovo_stato == 2) {
        snprintf(query, sizeof(query), 
                 "UPDATE dispositivi SET stato = %d, timestamp_attivazione = NOW() + INTERVAL %d HOUR WHERE nome = '%s'", 
                 nuovo_stato, ore, nome);

        // Invia la richiesta CoAP
        invia_richiesta_coap(nome);
    } else {
        snprintf(query, sizeof(query), 
                 "UPDATE dispositivi SET stato = %d, timestamp_attivazione = NULL WHERE nome = '%s'", 
                 nuovo_stato, nome);
    }

    if (mysql_query(conn, query)) {
        printf("Errore nell'aggiornamento dello stato: %s\n", mysql_error(conn));
    } else {
        printf("Stato del dispositivo '%s' aggiornato con successo.\n", nome);
    }
}

void mostra_dati(MYSQL *conn) {
    MYSQL_RES *res;
    MYSQL_ROW row;

    if (mysql_query(conn, "SELECT timestamp, voltage, current, power FROM data")) {
        printf("Errore nella query: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        printf("Errore nel recupero dei risultati: %s\n", mysql_error(conn));
        return;
    }

    printf("Dati raccolti:\n");
    while ((row = mysql_fetch_row(res))) {
        printf("Timestamp: %s, Tensione: %s V, Corrente: %s A, Potenza: %s kW\n", row[0], row[1], row[2], row[3]);
    }

    mysql_free_result(res);
}

void invia_richiesta_coap(const char *nome_dispositivo) {
    static coap_endpoint_t server_ep;
    static coap_message_t request[1]; // CoAP request
    char payload[256];
    char *ip_address = "fd00::1"; // Indirizzo IPv6 del server CoAP
    char *port = "5683"; // Porta del server CoAP
    
    snprintf(payload, sizeof(payload), "%s", nome_dispositivo);

    // Configura l'endpoint del server CoAP
    coap_endpoint_parse("coap://[fd00::1]:5683", strlen("coap://[fd00::1]:5683"), &server_ep);

    // Inizializza il messaggio CoAP
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "gestione");
    coap_set_payload(request, (uint8_t *)payload, strlen(payload));

    // Invia la richiesta CoAP
    printf("Invio richiesta CoAP al server: %s\n", payload);
    COAP_BLOCKING_REQUEST(&server_ep, request, NULL);

    printf("Richiesta CoAP inviata con successo.\n");
}

void inserisci_dispositivo(MYSQL *conn) {
    char nome[255], tipo[255];
    float consumo_kwh;
    int durata;

    printf("Inserisci il nome del dispositivo: ");
    scanf("%s", nome);
    printf("Inserisci il tipo del dispositivo: ");
    scanf("%s", tipo);
    printf("Inserisci il consumo in kWh: ");
    scanf("%f", &consumo_kwh);
    printf("Inserisci la durata in minuti: ");
    scanf("%d", &durata);

    char query[512];
    snprintf(query, sizeof(query), 
             "INSERT INTO dispositivi (nome, tipo, stato, consumo_kwh, durata) VALUES ('%s', '%s', 0, %.2f, %d)", 
             nome, tipo, consumo_kwh, durata);

    if (mysql_query(conn, query)) {
        printf("Errore nell'inserimento del dispositivo: %s\n", mysql_error(conn));
    } else {
        printf("Dispositivo '%s' inserito con successo.\n", nome);
    }
}

void rimuovi_dispositivo(MYSQL *conn) {
    char nome[255];

    printf("Inserisci il nome del dispositivo da rimuovere: ");
    scanf("%s", nome);

    // Verifica se il dispositivo esiste
    if (!dispositivo_esiste(conn, nome)) {
        printf("Errore: Il dispositivo '%s' non esiste nel database.\n", nome);
        return;
    }

    // Query per rimuovere il dispositivo
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM dispositivi WHERE nome = '%s'", nome);

    if (mysql_query(conn, query)) {
        printf("Errore nella rimozione del dispositivo: %s\n", mysql_error(conn));
    } else {
        printf("Dispositivo '%s' rimosso con successo.\n", nome);
    }
}

int main() {
    MYSQL *conn;
    int scelta;
    char nome_dispositivo[255];
    int nuovo_stato;
    int deadline = 0;
    conn = mysql_init(NULL);
    if (conn == NULL) {
        printf("Errore nell'inizializzazione di MySQL\n");
        return 1;
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        printf("Errore nella connessione al database: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    do {
        printf("\nGestione da terminale:\n");
        printf("1. Mostra dispositivi\n");
        printf("2. Cambia stato dispositivo\n");
        printf("3. Mostra dati raccolti\n");
        printf("4. Inserisci nuovo dispositivo\n");
        printf("5. Rimuovi dispositivo\n");
        printf("0. Esci\n");
        printf("Scelta: ");
        scanf("%d", &scelta);

        switch (scelta) {
            case 1:
                mostra_dispositivi(conn);
                break;
            case 2:
                printf("Inserisci il nome del dispositivo: ");
                scanf("%s", nome_dispositivo);
                printf("Inserisci il nuovo stato (2=Pronto, 1=Attivo, 0=Inattivo): ");
                scanf("%d", &nuovo_stato);

                if (nuovo_stato < 0 || nuovo_stato > 2) {
                    printf("Errore: Stato non valido. Deve essere 2, 1 o 0.\n");
                    break;
                }          
                if (nuovo_stato == 2) {
                    printf("Inserisci entro quante ore deve essere completata la task: ");
                    scanf("%d", &deadline);
                } else {
                    deadline = 0; // Non necessario per stati diversi da 2
                }

                cambia_stato_dispositivo(conn, nome_dispositivo, nuovo_stato, deadline);
                break;
            case 3:
                mostra_dati(conn);
                break;
            case 4:
                inserisci_dispositivo(conn);
                break;
            case 5:
                rimuovi_dispositivo(conn);
                break;
            case 0:
                printf("Uscita...\n");
                break;
            default:
                printf("Scelta non valida.\n");
        }
    } while (scelta != 0);

    mysql_close(conn);
    return 0;
}