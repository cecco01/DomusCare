import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout
import emlearn
import mysql.connector  # Libreria per interagire con MySQL

# Caricamento del dataset
data = pd.read_csv('smart_grid_dataset.csv')  # Sostituisci con il percorso corretto del dataset

# Preprocessing dei dati
data['time'] = pd.to_datetime(data['Timestamp'])  # Conversione della colonna Timestamp in datetime
data['hour'] = data['time'].dt.hour  # Estrazione dell'ora dal timestamp
data['day'] = data['time'].dt.day  # Estrazione del giorno
data['month'] = data['time'].dt.month  # Estrazione del mese
data['year'] = data['time'].dt.year  # Estrazione dell'anno

# Selezione delle feature: data (giorno, mese, anno), ora, tensione, corrente, prezzo dell'energia
X = data[['year', 'month', 'day', 'hour', 'Voltage (V)', 'Current (A)', 'Electricity Price (USD/kWh)']]
y_consumption = data['Power Consumption (kW)']  # Target: potenza consumata
y_solar = data['Solar Power (kW)']  # Target: potenza emessa dai pannelli solari

# Connessione al database MySQL
conn = mysql.connector.connect(
    host="localhost",       # Sostituisci con l'host del tuo server MySQL
    user="Luca",      # Sostituisci con il tuo nome utente MySQL
    password="Luca",# Sostituisci con la tua password MySQL
    database="iot"          # Sostituisci con il nome del tuo database
)
cursor = conn.cursor()

# Creazione della tabella se non esiste
cursor.execute('''
CREATE TABLE IF NOT EXISTS data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    year INT NOT NULL,
    month INT NOT NULL,
    day INT NOT NULL,
    hour INT NOT NULL,
    voltage FLOAT NOT NULL,
    current FLOAT NOT NULL,
    price FLOAT NOT NULL,
    power FLOAT NOT NULL
)
''')

# Inserimento dei dati nel database
for _, row in data.iterrows():
    cursor.execute('''
    INSERT INTO data (year, month, day, hour, voltage, current, price, power)
    VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
    ''', (row['year'], row['month'], row['day'], row['hour'], row['Voltage (V)'], row['Current (A)'], row['Electricity Price (USD/kWh)'], row['Power Consumption (kW)']))

conn.commit()
conn.close()
print("Dati caricati nel database con successo!")

# Suddivisione in training e test set
X_train, X_test, y_train_consumption, y_test_consumption = train_test_split(X, y_consumption, test_size=0.25, random_state=42)
_, _, y_train_solar, y_test_solar = train_test_split(X, y_solar, test_size=0.25, random_state=42)

# Creazione del modello di rete neurale per la potenza consumata
model_consumption = Sequential([
    Dense(64, activation='relu', input_shape=(X_train.shape[1],)),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione della potenza consumata
])

model_consumption.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello per la potenza consumata
model_consumption.fit(X_train, y_train_consumption, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello per la potenza consumata
loss_consumption, mae_consumption = model_consumption.evaluate(X_test, y_test_consumption)
print(f"Mean Absolute Error (Consumo): {mae_consumption}")

# Creazione del modello di rete neurale per la potenza solare
model_solar = Sequential([
    Dense(64, activation='relu', input_shape=(X_train.shape[1],)),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione della potenza solare
])

model_solar.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello per la potenza solare
model_solar.fit(X_train, y_train_solar, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello per la potenza solare
loss_solar, mae_solar = model_solar.evaluate(X_test, y_test_solar)
print(f"Mean Absolute Error (Potenza Solare): {mae_solar}")

# Conversione dei modelli in formato C per dispositivi IoT
path_consumption = 'smart_grid_model_consumption.h'
cmodel_consumption = emlearn.convert(model_consumption, method='inline')
cmodel_consumption.save(file=path_consumption, name='smart_grid_model_consumption')

path_solar = 'smart_grid_model_solar.h'
cmodel_solar = emlearn.convert(model_solar, method='inline')
cmodel_solar.save(file=path_solar, name='smart_grid_model_solar')

print(f"Modelli convertiti e salvati in formato C: {path_consumption}, {path_solar}")