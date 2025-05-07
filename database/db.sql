-- Drop the database if it exists
DROP DATABASE IF EXISTS iot;

-- Create the database
CREATE DATABASE iot;

-- Use the created database
USE iot;

-- Drop the sensor table if it exists
DROP TABLE IF EXISTS sensor;

-- Create the sensor table
CREATE TABLE sensor (
    ip_address VARCHAR(45) PRIMARY KEY,
    type VARCHAR(45),
    status INT,
    registration_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Drop the data table if it exists
DROP TABLE IF EXISTS data;

-- Create the data table
CREATE TABLE data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- Timestamp dei dati
    year INT NOT NULL,                             -- Anno
    month INT NOT NULL,                            -- Mese
    day INT NOT NULL,                              -- Giorno
    hour INT NOT NULL,                             -- Ora
    voltage FLOAT NOT NULL,                        -- Tensione (V)
    current FLOAT NOT NULL,                        -- Corrente (A)
    power FLOAT NOT NULL,                          -- Potenza consumata (kW)
    price FLOAT NOT NULL                           -- Prezzo dell'energia (USD/kWh)
);

-- Drop the failures table if it exists
DROP TABLE IF EXISTS failures;

-- Create the failures table
CREATE TABLE failures (
    id INT AUTO_INCREMENT PRIMARY KEY,
    component VARCHAR(45),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Drop the dispositivi table if it exists
DROP TABLE IF EXISTS dispositivi;

-- Create the dispositivi table
CREATE TABLE dispositivi (
    nome VARCHAR(255) PRIMARY KEY,
    tipo VARCHAR(255) NOT NULL,
    stato INT NOT NULL DEFAULT 0,  -- 2=pronto, 1=attivo, 0=inattivo
    consumo_kwh FLOAT NOT NULL,    -- Consumo in kWh
    durata INT NOT NULL            -- Durata in minuti
);

-- Inserimento di dati plausibili nella tabella dispositivi
INSERT INTO dispositivi (nome, tipo, stato, consumo_kwh, durata) VALUES
('Lavatrice', 'Lavatrice', 0, 1.5, 60),
('Asciugatrice', 'Asciugatrice', 0, 2.0, 90),
('Lavastoviglie', 'Lavastoviglie', 0, 1.2, 45);

