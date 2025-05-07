import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout
import emlearn

# Caricamento del dataset
data = pd.read_csv('smart_grid_dataset.csv')  # Sostituisci con il percorso corretto del dataset

# Preprocessing dei dati
data['time'] = pd.to_datetime(data['time'])
data['hour'] = data['time'].dt.hour
data['day_of_week'] = data['time'].dt.dayofweek

# Selezione delle feature: time (ora), voltage, corrente, potenza
X = data[['hour', 'voltage', 'current', 'power']]  # Sostituisci con i nomi corretti delle colonne
y = data['load']  # Target: carico energetico

# Suddivisione in training e test set
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Creazione del modello di rete neurale
model = Sequential([
    Dense(64, activation='relu', input_shape=(X_train.shape[1],)),
    Dropout(0.2),
    Dense(32, activation='relu'),
    Dense(1)  # Output: previsione del carico
])

model.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Addestramento del modello
model.fit(X_train, y_train, epochs=100, batch_size=32, validation_split=0.2)

# Valutazione del modello
loss, mae = model.evaluate(X_test, y_test)
print(f"Mean Absolute Error: {mae}")

# Conversione del modello in formato C per dispositivi IoT
path = 'smart_grid_model.h'
cmodel = emlearn.convert(model, method='inline')
cmodel.save(file=path, name='smart_grid_model')

print(f"Modello convertito e salvato in formato C: {path}")