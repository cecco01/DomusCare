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
def on_connect_pm25(client, userdata, flags, rc):
    print("Connesso al topic PM2.5 con codice: " + str(rc))
    client.subscribe("airclean/pm25")

def on_connect_co2(client, userdata, flags, rc):
    print("Connesso al topic CO2 con codice: " + str(rc))
    client.subscribe("airclean/co2")

def on_connect_temperature(client, userdata, flags, rc):
    print("Connesso al topic Temperatura con codice: " + str(rc))
    client.subscribe("airclean/temperature")

# ------------------------------------------------------------------------------------
# Callback per la ricezione dei messaggi
def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto dal topic {msg.topic}: {msg.payload.decode('utf-8', 'ignore')}")
    
    # Decodifica del messaggio JSON
    json_payload = json.loads(msg.payload.decode("utf-8", "ignore"))
    value = float(json_payload.get("value", 0))
    timestamp = json_payload.get("timestamp", None)

    # Inserimento dei dati nel database
    if msg.topic == "airclean/pm25":
        val_pm25 = (value, "PM2.5", timestamp)
        mycursor.execute(sql, val_pm25)
    elif msg.topic == "airclean/co2":
        val_co2 = (value, "CO2", timestamp)
        mycursor.execute(sql, val_co2)
    elif msg.topic == "airclean/temperature":
        val_temp = (value, "Temperature", timestamp)
        mycursor.execute(sql, val_temp)

    # Conferma delle modifiche nel database
    mydb.commit()

# ------------------------------------------------------------------------------------
# Mappatura dei topic ai callback di connessione
on_connect_callbacks = {
    "pm25": on_connect_pm25,
    "co2": on_connect_co2,
    "temperature": on_connect_temperature
}

# Configurazione del client MQTT
def mqtt_client(topic):
    client = mqtt.Client()
    client.on_connect = on_connect_callbacks[topic]
    client.on_message = on_message
    client.connect("127.0.0.1", 1883, 60)  # Connessione al broker MQTT
    client.loop_forever()  # Loop infinito per ricevere messaggi

# ------------------------------------------------------------------------------------
# Funzione principale
def main():
    thread_pm25 = Thread(target=mqtt_client, args=("pm25",))
    thread_co2 = Thread(target=mqtt_client, args=("co2",))
    thread_temperature = Thread(target=mqtt_client, args=("temperature",))
    
    # Avvio dei thread
    thread_pm25.start()
    thread_co2.start()
    thread_temperature.start()

if __name__ == '__main__':
    main()