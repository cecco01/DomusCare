import paho.mqtt.client as mqtt
from threading import Thread
import mysql.connector
import json

# Connessione al database MySQL
mydb = mysql.connector.connect(
    host="localhost",
    user="root",
    password="password",
    database="AirCleanDB"
)

mycursor = mydb.cursor()

# Query SQL per inserire i dati
sql = "INSERT INTO air_quality_data (value, type, timestamp) VALUES (%s, %s, %s)"

# ------------------------------------------------------------------------------------
# Callback per la connessione al broker MQTT
def on_connect(client, userdata, flags, rc):
    print("Connesso al broker MQTT con codice: " + str(rc))
    client.subscribe("sensor/temp_co2_sal")  # Iscrizione al topic del sensore

# ------------------------------------------------------------------------------------
# Callback per la ricezione dei messaggi
def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto dal topic {msg.topic}: {msg.payload.decode('utf-8', 'ignore')}")
    
    # Decodifica del messaggio JSON
    try:
        json_payload = json.loads(msg.payload.decode("utf-8", "ignore"))
        temperature = float(json_payload.get("temperature", 0))
        co2 = float(json_payload.get("co2", 0))
        pm = float(json_payload.get("pm", 0))
        timestamp = json_payload.get("timestamp", None)  # Aggiungi timestamp se presente

        # Inserimento dei dati nel database
        val_temp = (temperature, "Temperature", timestamp)
        val_co2 = (co2, "CO2", timestamp)
        val_pm = (pm, "PM2.5", timestamp)

        mycursor.execute(sql, val_temp)
        mycursor.execute(sql, val_co2)
        mycursor.execute(sql, val_pm)

        # Conferma delle modifiche nel database
        mydb.commit()
        print("Dati salvati nel database con successo.")

    except json.JSONDecodeError as e:
        print(f"Errore nella decodifica del messaggio JSON: {e}")
    except Exception as e:
        print(f"Errore durante l'inserimento nel database: {e}")

# ------------------------------------------------------------------------------------
# Configurazione del client MQTT
def mqtt_client():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect("127.0.0.1", 1883, 60)  # Connessione al broker MQTT
    client.loop_forever()  # Loop infinito per ricevere messaggi

# ------------------------------------------------------------------------------------
# Funzione principale
def main():
    thread = Thread(target=mqtt_client)
    thread.start()

if __name__ == '__main__':
    main()