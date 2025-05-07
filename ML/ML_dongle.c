#include <stdio.h>
#include "smart_grid_model.h" // Include il modello generato

int main() {
    // Input di esempio: year, month, day, hour, Voltage (V), Current (A), Power Consumption (kW)
    float input_features[] = {2025, 5, 7, 14, 230.5, 10.2, 2.5}; // Sostituisci con i tuoi dati reali

    // Variabile per memorizzare l'output del modello
    float output;

    // Esecuzione del modello
    int result = smart_grid_model_predict(input_features, &output);

    // Controllo del risultato
    if (result == 0) {
        printf("Previsione del consumo di potenza: %.2f kW\n", output);
    } else {
        printf("Errore durante l'esecuzione del modello\n");
    }

    return 0;
}