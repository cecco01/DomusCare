import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout
import emlearn
import sqlite3  # Libreria per interagire con SQLite

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
y = data['Power Consumption (kW)']  # Target: potenza consumata

# Caricamento dei dati nel database SQLite
conn = sqlite3.connect('database.db')  # Connessione al database SQLite
cursor = conn.cursor()

# Creazione della tabella se non esiste
cursor.execute('''
CREATE TABLE IF NOT EXISTS data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    hour INTEGER NOT NULL,
    voltage REAL NOT NULL,
    current REAL NOT NULL,
    price REAL NOT NULL,
    power REAL NOT NULL
)
''')

# Inserimento dei dati nel database
for _, row in data.iterrows():
    cursor.execute('''
    INSERT INTO data (year, month, day, hour, voltage, current, price, power)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    ''', (row['year'], row['month'], row['day'], row['hour'], row['Voltage (V)'], row['Current (A)'], row['Electricity Price (USD/kWh)'], row['Power Consumption (kW)']))

conn.commit()
conn.close()
print("Dati caricati nel database con successo!")

# Suddivisione in training e test set
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Creazione del modello di rete neurale
model = Sequential([
    Dense(64, activation='relu', input_shape=(X_train.shape[1],)),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione della potenza consumata
])

model.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello
model.fit(X_train, y_train, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello
loss, mae = model.evaluate(X_test, y_test)
print(f"Mean Absolute Error: {mae}")

# Conversione del modello in formato C per dispositivi IoT
path = 'smart_grid_model.h'
cmodel = emlearn.convert(model, method='inline')
cmodel.save(file=path, name='smart_grid_model')

print(f"Modello convertito e salvato in formato C: {path}")