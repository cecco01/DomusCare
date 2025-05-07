-- Creazione del database
CREATE DATABASE AirCleanDB;

-- Utilizzo del database
USE AirCleanDB;

-- Creazione della tabella per i dati sulla qualità dell'aria
CREATE TABLE air_quality_data (
    id INT AUTO_INCREMENT PRIMARY KEY, -- Identificativo univoco
    value FLOAT NOT NULL,              -- Valore del dato (es. PM2.5, CO2, temperatura)
    type VARCHAR(50) NOT NULL,         -- Tipo di dato (es. "PM2.5", "CO2", "Temperature")
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP -- Timestamp del dato
);