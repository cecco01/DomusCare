#include <stdio.h>
#include <sqlite3.h> // Libreria per SQLite
#include "smart_grid_model.h" // Include il modello generato

#define DB_PATH "database.db" // Percorso al database SQLite
#define QUERY "SELECT year, month, day, hour, voltage, current, power FROM data_table ORDER BY id DESC LIMIT 100"

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    float input_features[7]; // Array per memorizzare i dati di input
    float output;

    // Apertura del database
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        printf("Errore nell'apertura del database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Preparazione della query
    if (sqlite3_prepare_v2(db, QUERY, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Errore nella preparazione della query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Iterazione sui risultati della query
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Lettura dei valori dalla riga corrente
        input_features[0] = sqlite3_column_double(stmt, 0); // year
        input_features[1] = sqlite3_column_double(stmt, 1); // month
        input_features[2] = sqlite3_column_double(stmt, 2); // day
        input_features[3] = sqlite3_column_double(stmt, 3); // hour
        input_features[4] = sqlite3_column_double(stmt, 4); // voltage
        input_features[5] = sqlite3_column_double(stmt, 5); // current
        input_features[6] = sqlite3_column_double(stmt, 6); // power

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
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}