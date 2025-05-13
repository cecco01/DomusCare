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
        print(f"RENDER POST - Request parameters: {request.uri_query}")
        try:
            # Parsing del payload JSON
            payload = json.loads(request.payload)
            sensor_type = payload.get("t")
            ip_address = request.source #payload.get("ip_address")

            print(f"Payload ricevuto: {request.payload}")
            print(f"Dimensione del payload: {len(request.payload)}")
            print(f"Tipo: {sensor_type}, IP: {ip_address}")

            if not sensor_type or not ip_address:
                print("Tipo o indirizzo IP mancante.")
                self.payload = "Errore: tipo o indirizzo IP mancante."
                return self

            # Inserisce il sensore nel database
            print(f"tipo: {sensor_type}, ip_address: {ip_address}")
            self.register_sensor(sensor_type, ip_address, payload)

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
            SELECT ip_address, type
            FROM sensor
            WHERE type = %s
            """
            cursor.execute(select_sensor_query, (type,))

            sensor_data = cursor.fetchall()
            cursor.close()

            sensor = {}

            if sensor_data:
                for row in sensor_data:
                    ip_address, type = row
                    sensor[type] = {"ip_address": ip_address}

                
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

    def register_sensor(self, sensor_type, ip_address, payload):
        """
        Registra un sensore nel database e invia un messaggio agli attuatori non attivi.

        :param sensor_type: Tipo del sensore.
        :param ip_address: Indirizzo IP del sensore.
        :param payload: Dati JSON ricevuti per il sensore.
        """
        if not self.connection.is_connected():
            raise Error("Connessione al database persa.")
        print(f'Creo il cursore, ip_address: {ip_address}, sensor_type: {sensor_type}')
        cursor = self.connection.cursor()
        query = """
        INSERT INTO sensor (type, ip_address)
        VALUES (%s, %s)
        """
        cursor.execute(query, (str(sensor_type), str(ip_address)))
        self.connection.commit()
        print(f"Registrato il sensore di tipo {sensor_type} con indirizzo IP {ip_address}.")
        # Se il sensore è un actuator, aggiungilo anche nella tabella dispositivi
        if sensor_type == "actuator":
            dispositivo_query = """
            INSERT INTO dispositivi (nome, stato, consumo_kwh, durata)
            VALUES (%s, %s, %s, %s)
            """
            # Estrai i dati dal payload JSON
            nome_dispositivo = payload.get("n")
            stato_dispositivo = payload.get("s")
            consumo_kwh = payload.get("c")
            durata_task = payload.get("d")
            cursor.execute(dispositivo_query, (nome_dispositivo, stato_dispositivo, consumo_kwh, durata_task))
            self.connection.commit()
            print(f"Registrato il dispositivo {nome_dispositivo} con stato {stato_dispositivo}, consumo {consumo_kwh} kWh e durata {durata_task} ore.")
        cursor.close()

        # Invia il messaggio agli attuatori non attivi
        if sensor_type in ["solar", "power"]:
            self.notify_actuators()

    def notify_actuators(self):
        """
        Recupera gli attuatori non attivi dal database e invia loro un messaggio di attivazione.
        """
        if not self.connection.is_connected():
            raise Error("Connessione al database persa.")

        cursor = self.connection.cursor()
        query = """
        SELECT ip_address FROM sensor
        WHERE type = 'actuator'
        """
        cursor.execute(query)
        actuators = cursor.fetchall()
        cursor.close()

        for actuator in actuators:
            actuator_ip = actuator[0]
            self.send_activation_message(actuator_ip)

    def send_activation_message(self, ip_address):
        """
        Invia un messaggio CoAP al dispositivo Smart Plug per attivarlo con data, ora e indirizzi IP dei sensori.

        :param ip_address: Indirizzo IP del dispositivo Smart Plug.
        """
        port = 5683
        client = HelperClient(server=(ip_address, port))

        try:
            # Ottieni la data e l'ora correnti
            now = datetime.now()

            # Recupera gli indirizzi IP dei sensori dal database
            cursor = self.connection.cursor()
            query = """
            SELECT ip_address, type FROM sensor WHERE type IN ('solar', 'power')
            """
            cursor.execute(query)
            sensors = cursor.fetchall()
            cursor.close()

            # Organizza gli indirizzi IP dei sensori
            solar_ip = None
            power_ip = None
            for sensor in sensors:
                if sensor[1] == "solar":
                    solar_ip = sensor[0]
                elif sensor[1] == "power":
                    power_ip = sensor[0]

            # Crea il payload JSON
            payload = {
                "tipo": 0,
                "ora": now.hour,
                "minuti": now.minute,
                "giorno": now.day,
                "mese": now.month,
                "solar_ip": solar_ip,
                "power_ip": power_ip
            }

            # Invia il messaggio CoAP
            print(f"Invio messaggio CoAP a {ip_address} con payload: {payload}")
            response = client.post("gestione", json.dumps(payload))
            if response:
                print(f"Messaggio inviato al dispositivo Smart Plug: {payload}")
            else:
                print("Errore durante l'invio del messaggio al dispositivo Smart Plug.")

        except Exception as e:
            print(f"Errore durante l'invio del messaggio CoAP: {e}")
        finally:
            client.stop()

