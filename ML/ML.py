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
y = data['Power Consumption (kW)']  # Target: potenza consumata

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