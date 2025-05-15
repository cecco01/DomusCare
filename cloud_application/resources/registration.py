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
            print(f"Payload ricevuto: {payload}")
            sensor_type = payload.get("t")
            ip_address = request.source #payload.get("ip_address")
            if sensor_type =="a":
                sensor_type== "actuator"
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

            query = """
            INSERT INTO dispositivi (nome, consumo_kwh,durata)
            VALUES (%s, %s, %s)
            """
            # Estrai i dati dal payload
            name = payload.get("n")
            consumo_kwh = payload.get("c")
            durata = payload.get("d")
            # Verifica se i dati sono presenti e validi
            if not name or not consumo_kwh or not durata:
                print(f"Errore: Dati mancanti nel payload. name={name}, consumo_kwh={consumo_kwh}, durata={durata}")

                
            # Esegui l'inserimento nella tabella dispositivi
            cursor.execute(query, (str(name), float(consumo_kwh), int(durata)))
            
            self.connection.commit()
            print(f"Registrato il dispositivo di tipo {sensor_type} con indirizzo IP {ip_address}.")

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
        client = None  # Inizializza la variabile client
        try:
            # Se ip_address è una stringa, convertila in una tupla
            if isinstance(ip_address, str):
                ip_address = eval(ip_address)  # Converte la stringa in una tupla

            # Estrai l'indirizzo IP e la porta dalla tupla
            if isinstance(ip_address, tuple) and len(ip_address) == 2:
                ip, port = ip_address
            else:
                raise ValueError(f"Formato non valido per ip_address: {ip_address}")

            print(f"Invio messaggio di attivazione al dispositivo con IP: {ip} e porta: {port}")

            # Recupera gli indirizzi IP di solar e power dal database
            cursor = self.connection.cursor()
            query = """
            SELECT type, ip_address FROM sensor
            WHERE type IN ('solar', 'power')
            """
            cursor.execute(query)
            sensors = cursor.fetchall()
            cursor.close()

            # Inizializza le variabili per gli IP
            solar_ip = ""
            power_ip = ""

            # Assegna gli indirizzi IP in base al tipo
            for sensor_type, ip_port in sensors:
                if isinstance(ip_port, tuple) and len(ip_port) == 2:  # Verifica che ip_port sia una tupla valida
                    sensor_ip, sensor_port = ip_port
                    formatted_ip = f"coap://[{sensor_ip}]:{sensor_port}"  # Formatta l'indirizzo IP e la porta
                    if sensor_type == "solar":
                        solar_ip = formatted_ip
                        print(f"Solar IP: {solar_ip}")
                    elif sensor_type == "power":
                        power_ip = formatted_ip
                        print(f"Power IP: {power_ip}")
                else:
                    print(f"Formato non valido per ip_port: {ip_port}")

            # Log degli indirizzi IP (anche se vuoti)
            print(f"INVIO DEGLI INDIRIZII : Indirizzo IP Solar: {solar_ip}, Indirizzo IP Power: {power_ip}")

            # Crea il client CoAP
            client = HelperClient(server=(ip, port))

            # Costruisci il payload
            payload = {
                "ora": datetime.now().hour,
                "minuti": datetime.now().minute,
                "giorno": datetime.now().day,
                "mese": datetime.now().month,
                "solar_ip": solar_ip,
                "power_ip": power_ip
            }

            # Invia il messaggio CoAP
            response = client.post("activation", json.dumps(payload))
            if response:
                print(f"Messaggio inviato al dispositivo Smart Plug: {payload}")
            else:
                print("Errore durante l'invio del messaggio al dispositivo Smart Plug.")

        except Exception as e:
            print(f"Errore durante l'invio del messaggio CoAP: {e}")
        finally:
            if client:
                client.stop()


