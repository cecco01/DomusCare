import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout, Input
import emlearn

# Caricamento del dataset
data = pd.read_csv('smart_grid_dataset.csv')  # Sostituisci con il percorso corretto del dataset

# Preprocessing dei dati
data['time'] = pd.to_datetime(data['Timestamp'])  # Conversione della colonna Timestamp in datetime
data['hour'] = data['time'].dt.hour  # Estrazione dell'ora dal timestamp
data['minute'] = data['time'].dt.minute  # Estrazione dei minuti
data['day'] = data['time'].dt.day  # Estrazione del giorno
data['month'] = data['time'].dt.month  # Estrazione del mese

# Calcolo dei valori futuri (spostamento di 1 ora nel futuro)
data['Future Solar Power (kW)'] = data['Solar Power (kW)'].shift(-4)  # Supponendo 15 minuti tra le righe
data['Future Power Consumption (kW)'] = data['Power Consumption (kW)'].shift(-4)

# Rimuovi le righe con valori NaN generati dallo shift
data = data.dropna()

# Modello 1: Predizione della produzione futura
X_production = data[['month', 'day', 'hour', 'minute', 'Solar Power (kW)']]  # Input
y_production = data['Future Solar Power (kW)']  # Target: produzione futura

# Modello 2: Predizione del consumo futuro
X_consumption = data[['month', 'day', 'hour', 'minute', 'Power Consumption (kW)']]  # Input
y_consumption = data['Future Power Consumption (kW)']  # Target: consumo futuro

# Suddivisione in training e test set
X_train_prod, X_test_prod, y_train_prod, y_test_prod = train_test_split(X_production, y_production, test_size=0.25, random_state=42)
X_train_cons, X_test_cons, y_train_cons, y_test_cons = train_test_split(X_consumption, y_consumption, test_size=0.25, random_state=42)

# Standardizzazione delle feature
scaler_prod = StandardScaler()
X_train_prod = scaler_prod.fit_transform(X_train_prod)
X_test_prod = scaler_prod.transform(X_test_prod)

scaler_cons = StandardScaler()
X_train_cons = scaler_cons.fit_transform(X_train_cons)
X_test_cons = scaler_cons.transform(X_test_cons)

# Creazione del modello per la produzione futura
model_production = Sequential([
    Input(shape=(X_train_prod.shape[1],)),  # Definizione esplicita dell'input
    Dense(64, activation='relu'),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione della produzione futura
])

model_production.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello per la produzione futura
model_production.fit(X_train_prod, y_train_prod, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello per la produzione futura
loss_prod, mae_prod = model_production.evaluate(X_test_prod, y_test_prod)
print(f"Mean Absolute Error (Produzione Futura): {mae_prod}")

# Creazione del modello per il consumo futuro
model_consumption = Sequential([
    Input(shape=(X_train_cons.shape[1],)),  # Definizione esplicita dell'input
    Dense(64, activation='relu'),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione del consumo futuro
])

model_consumption.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello per il consumo futuro
model_consumption.fit(X_train_cons, y_train_cons, epochs=50, batch_size=32, validation_split=0.2)

# Valutazione del modello per il consumo futuro
loss_cons, mae_cons = model_consumption.evaluate(X_test_cons, y_test_cons)
print(f"Mean Absolute Error (Consumo Futuro): {mae_cons}")

# Salvataggio dei modelli in formato .h
path_production = 'smart_grid_model_production.h'
path_consumption = 'smart_grid_model_consumption.h'

# Converti il modello per la produzione futura in formato C
cmodel_production = emlearn.convert(model_production, method='inline')
cmodel_production.save(file=path_production, name='model_production')
print(f"Modello per la produzione futura salvato in: {path_production}")

# Converti il modello per il consumo futuro in formato C
cmodel_consumption = emlearn.convert(model_consumption, method='inline')
cmodel_consumption.save(file=path_consumption, name='model_consumption')
print(f"Modello per il consumo futuro salvato in: {path_consumption}")