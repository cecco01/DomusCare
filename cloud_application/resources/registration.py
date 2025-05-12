from mysql.connector import Error
from coapthon.resources.resource import Resource
from core.database import Database
from coapthon.client.helperclient import HelperClient
import json
import time
import threading
import re
from coapthon import defines
from datetime import datetime

class Registration(Resource):

    def __init__(self, name="Registration"):
        super(Registration, self).__init__(name)
        self.payload = "Registration Resource"
        self.database = Database()
        self.connection = self.database.connect_db()

    def render_GET(self, request):
        print(f"RENDER GET")
        # Print parameters of the request
        
        print(f"Request parameters: {request.uri_query}")
    
        # Take the value of the query parameter
        query = request.uri_query

        self.fetch_sensor_from_db(query)

        return self

    def render_POST(self, request):
        """
        Gestisce la registrazione di un sensore. Se il tipo è 'actuator',
        invia un messaggio CoAP al dispositivo Smart Plug con data e ora.
        """
        try:
            # Parsing del payload JSON
            payload = json.loads(request.payload)
            sensor_type = payload.get("type")
            ip_address = payload.get("ip_address")

            if not sensor_type or not ip_address:
                self.payload = "Errore: tipo o indirizzo IP mancante."
                return self

            # Inserisce il sensore nel database
            self.register_sensor(sensor_type, ip_address)

            # Se il tipo è 'actuator', invia un messaggio CoAP al dispositivo Smart Plug
            if sensor_type == "actuator":
                self.send_activation_message(ip_address)

            self.payload = f"Registrazione completata per il sensore di tipo {sensor_type}."
            return self

        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            return self
        except Error as e:
            self.payload = f"Errore durante la registrazione del sensore: {e}"
            return self

    def fetch_sensor_from_db(self, type):
        if not self.connection.is_connected():
            self.payload = None
            print("Database connection lost, Payload: None")
            return self
 
        try:
            cursor = self.connection.cursor()
            select_sensor_query = """
            SELECT ip_address, type, status
            FROM sensor
            WHERE type = %s
            """
            cursor.execute(select_sensor_query, (type,))

            sensor_data = cursor.fetchall()
            cursor.close()

            sensor = {}

            if sensor_data:
                for row in sensor_data:
                    ip_address, type, status = row
                    sensor[type] = {"status": int(status), "ip_address": ip_address}

                if sensor[type]["status"] == 1:
                    response = {
                        "sensor": type,
                        "ip_address": sensor[type]["ip_address"]
                    }
                    self.payload = json.dumps(response, separators=(',', ':'))
                    print(f"Payload: {self.payload}")

            else:
                self.payload = None
                print(f"No sensor found for type: {type}. Payload: {self.payload}")
        
        except Error as e:
            self.payload = None
            print(f"Error retrieving sensor data: {e}, Payload: {self.payload}")

    def register_sensor(self, sensor_type, ip_address):
        """
        Registra un sensore nel database.

        :param sensor_type: Tipo del sensore.
        :param ip_address: Indirizzo IP del sensore.
        """
        if not self.connection.is_connected():
            raise Error("Connessione al database persa.")

        cursor = self.connection.cursor()
        query = """
        INSERT INTO sensor (type, ip_address, status)
        VALUES (%s, %s, %s)
        """
        cursor.execute(query, (sensor_type, ip_address, 0))
        self.connection.commit()
        cursor.close()

    def send_activation_message(self, ip_address):
        """
        Invia un messaggio CoAP al dispositivo Smart Plug per attivarlo con data e ora.

        :param ip_address: Indirizzo IP del dispositivo Smart Plug.
        """
        port = 5683
        client = HelperClient(server=(ip_address, port))

        try:
            # Ottieni la data e l'ora correnti
            now = datetime.now()
            payload = {
                "tipo": 0,
                "ora": now.hour,
                "minuti": now.minute,
                "giorno": now.day,
                "mese": now.month
            }

            # Invia il messaggio CoAP
            response = client.post("gestione", json.dumps(payload))
            if response:
                print(f"Messaggio inviato al dispositivo Smart Plug: {payload}")
            else:
                print("Errore durante l'invio del messaggio al dispositivo Smart Plug.")

        except Exception as e:
            print(f"Errore durante l'invio del messaggio CoAP: {e}")
        finally:
            client.stop()

