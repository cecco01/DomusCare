-- Creazione del database e della tabella
CREATE DATABASE IF NOT EXISTS SMART_HOME; -- Crea il database se non esiste già
USE SMART_HOME; -- Seleziona il database

CREATE TABLE IF NOT EXISTS data_table (
    id INTEGER PRIMARY KEY AUTOINCREMENT, -- ID univoco per ogni riga
    year INTEGER NOT NULL,                -- Anno
    month INTEGER NOT NULL,               -- Mese
    day INTEGER NOT NULL,                 -- Giorno
    hour INTEGER NOT NULL,                -- Ora
    voltage REAL NOT NULL,                -- Tensione (V)
    current REAL NOT NULL,                -- Corrente (A)
    power REAL NOT NULL                   -- Potenza (kW)
);

