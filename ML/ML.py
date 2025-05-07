import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout

# Caricamento del dataset
data = pd.read_csv('smart_grid_dataset.csv')  # Sostituisci con il percorso corretto del dataset

# Preprocessing dei dati
data['time'] = pd.to_datetime(data['Timestamp'])  # Conversione della colonna Timestamp in datetime
data['hour'] = data['time'].dt.hour  # Estrazione dell'ora dal timestamp
data['day'] = data['time'].dt.day  # Estrazione del giorno
data['month'] = data['time'].dt.month  # Estrazione del mese
data['year'] = data['time'].dt.year  # Estrazione dell'anno

# Selezione delle feature e dei target
X = data[['year', 'month', 'day', 'hour', 'Voltage (V)', 'Current (A)', 'Electricity Price (USD/kWh)']]
y_consumption = data['Power Consumption (kW)']  # Target: potenza consumata
y_solar = data['Solar Power (kW)']  # Target: potenza solare

# Suddivisione in training e test set
X_train, X_test, y_train_consumption, y_test_consumption = train_test_split(X, y_consumption, test_size=0.25, random_state=42)
_, _, y_train_solar, y_test_solar = train_test_split(X, y_solar, test_size=0.25, random_state=42)

# Standardizzazione delle feature
scaler = StandardScaler()
X_train = scaler.fit_transform(X_train)
X_test = scaler.transform(X_test)

# Creazione del modello per il consumo di potenza
model_consumption = Sequential([
    Dense(64, activation='relu', input_shape=(X_train.shape[1],)),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione della potenza consumata
])

model_consumption.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello per il consumo di potenza
model_consumption.fit(X_train, y_train_consumption, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello per il consumo di potenza
loss_consumption, mae_consumption = model_consumption.evaluate(X_test, y_test_consumption)
print(f"Mean Absolute Error (Consumo): {mae_consumption}")

# Creazione del modello per la potenza solare
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

# Salvataggio dei modelli
model_consumption.save('model_consumption.h5')
model_solar.save('model_solar.h5')
print("Modelli salvati come 'model_consumption.h5' e 'model_solar.h5'")