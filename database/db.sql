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
    solarpower FLOAT DEFAULT NULL,                     -- Potenza pannelli (kW)
    power FLOAT DEFAULT NULL                           -- Potenza consumata (kW)
    
);


-- Drop the dispositivi table if it exists
DROP TABLE IF EXISTS dispositivi;

-- Create the dispositivi table
CREATE TABLE dispositivi (
    nome VARCHAR(255) PRIMARY KEY,
    tipo VARCHAR(255) NOT NULL,
    stato INT NOT NULL DEFAULT 0,  -- 2=pronto, 1=attivo, 0=inattivo
    consumo_kwh FLOAT NOT NULL,    -- Consumo in kWh
    timestamp_attivazione TIMESTAMP DEFAULT NULL, -- Timestamp di attivazione
    durata INT NOT NULL            -- Durata in minuti
);

-- Inserimento di dati plausibili nella tabella dispositivi
INSERT INTO dispositivi (nome, tipo, stato, consumo_kwh, durata) VALUES
('Lavatrice', 'Lavatrice', 0, 1.5, 60),
('Asciugatrice', 'Asciugatrice', 0, 2.0, 90),
('Lavastoviglie', 'Lavastoviglie', 0, 1.2, 45);

-- Abilita il gestore di eventi MySQL (se non è già abilitato)
SET GLOBAL event_scheduler = ON;

-- Crea un evento per aggiornare lo stato dei dispositivi
DELIMITER $$

CREATE EVENT aggiorna_stato_dispositivi
ON SCHEDULE EVERY 1 MINUTE
DO
BEGIN
    UPDATE dispositivi
    SET stato = 0, timestamp_attivazione = NULL
    WHERE stato = 1 AND TIMESTAMPDIFF(MINUTE, timestamp_attivazione, NOW()) >= durata;
END$$

DELIMITER ;

-- Creazione del trigger per aggiornare il consumo totale
DELIMITER $$

CREATE TRIGGER aggiorna_consumo_dispositivi
AFTER INSERT ON data
FOR EACH ROW
BEGIN
    -- Calcola il consumo totale dei dispositivi attivi
    DECLARE consumo_totale FLOAT;
    SELECT SUM(consumo_kwh) INTO consumo_totale
    FROM dispositivi
    WHERE stato = 1;

    -- Aggiungi il consumo totale al valore di power nella nuova riga
    IF consumo_totale IS NOT NULL THEN
        UPDATE data
        SET power = NEW.power + consumo_totale
        WHERE id = NEW.id;
    END IF;
END$$

DELIMITER ;

