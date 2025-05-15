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
    ip_address VARCHAR(100) PRIMARY KEY,
    type VARCHAR(100) NOT NULL
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
    stato INT NOT NULL DEFAULT 0,  -- 2=pronto, 1=attivo, 0=inattivo
    consumo_kwh FLOAT NOT NULL,    -- Consumo in kWh
    timestamp_attivazione TIMESTAMP DEFAULT NULL, -- Timestamp di attivazione
    durata INT NOT NULL            -- Durata in minuti
);

-- Inserimento di dati plausibili nella tabella dispositivi


-- Abilita il gestore di eventi MySQL (se non è già abilitato)
SET GLOBAL event_scheduler = ON;

-- Crea un evento per aggiornare lo stato dei dispositivi
DELIMITER $$
DROP EVENT IF EXISTS aggiorna_stato_dispositivi;
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
DROP TRIGGER IF EXISTS aggiorna_consumo_dispositivi;
CREATE TRIGGER aggiorna_consumo_dispositivi
BEFORE INSERT ON data
FOR EACH ROW
BEGIN
    DECLARE consumo_totale FLOAT;
    SELECT SUM(consumo_kwh) INTO consumo_totale
    FROM dispositivi
    WHERE stato = 1;

    IF consumo_totale IS NOT NULL THEN
        SET NEW.power = IFNULL(NEW.power, 0) + consumo_totale;
    END IF;
END$$
DELIMITER ;
-- aggiungi  un trigger per inserire il tempo di attivazione 
DELIMITER $$
DROP TRIGGER IF EXISTS inserisci_tempo_attivazione;
CREATE TRIGGER inserisci_tempo_attivazione
BEFORE INSERT ON dispositivi
FOR EACH ROW
-- solo se il nuovo stato è 1
BEGIN
    IF NEW.stato = 1 THEN
        -- Se il nuovo stato è 1, imposta il timestamp di attivazione
        SET NEW.timestamp_attivazione = NOW();
    ELSE
        -- Altrimenti, imposta il timestamp di attivazione a NULL
        SET NEW.timestamp_attivazione = NULL;
    END IF; 
    SET NEW.timestamp_attivazione = NOW();
END$$
DELIMITER ;