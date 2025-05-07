#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h> // Libreria per interagire con MySQL
#include "smart_grid_model.h" // Include il modello generato

#define DB_HOST "localhost"    // Host del database
#define DB_USER "Luca"         // Nome utente MySQL
#define DB_PASS "Luca"         // Password MySQL
#define DB_NAME "iot"          // Nome del database

int main() {
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    // Connessione al database MySQL
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

    // Query per selezionare i dati dalla tabella
    if (mysql_query(conn, "SELECT year, month, day, hour, voltage, current, power FROM data")) {
        printf("Errore nella query: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        printf("Errore nel recupero dei risultati: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    float input_features[7]; // Array per memorizzare i dati di input
    float output;

    // Iterazione sui risultati della query
    while ((row = mysql_fetch_row(res))) {
        input_features[0] = atof(row[0]); // year
        input_features[1] = atof(row[1]); // month
        input_features[2] = atof(row[2]); // day
        input_features[3] = atof(row[3]); // hour
        input_features[4] = atof(row[4]); // voltage
        input_features[5] = atof(row[5]); // current
        input_features[6] = atof(row[6]); // power

        // Esecuzione del modello
        int result = smart_grid_model_predict(input_features, &output);

        // Controllo del risultato
        if (result == 0) {
            printf("Previsione del consumo di potenza: %.2f kW\n", output);
        } else {
            printf("Errore durante l'esecuzione del modello\n");
        }
    }

    // Pulizia
    mysql_free_result(res);
    mysql_close(conn);

    return 0;
}