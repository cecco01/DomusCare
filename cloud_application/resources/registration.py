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

    def ensure_connection(self):
        """
        Verifica e ripristina la connessione al database se necessario.
        """
        if not self.connection.is_connected():
            print("Connessione al database persa. Riconnessione in corso...")
            self.connection = self.database.connect_db()
            if not self.connection.is_connected():
                raise Error("Impossibile riconnettersi al database.")

    def render_GET(self, request):
        print(f"RENDER GET")
        print(f"Request parameters: {request.uri_query}")
        query = request.uri_query
        self.fetch_dongle_from_db(query)
        return self

    def render_POST(self, request):
        """
        Gestisce la registrazione di un donglee. Se il tipo è 'actuator',
        invia un messaggio CoAP al dispositivo Smart Plug con data e ora.
        """
        print(f"RENDER POST - Request parameters: {request.uri_query}")
        try:
            payload = json.loads(request.payload)
            print(f"Payload ricevuto: {payload}")
            dongle_type = payload.get("t")
            ip_address = request.source
            if dongle_type == "a":
                dongle_type = "actuator"

            if not dongle_type or not ip_address:
                print("Tipo o indirizzo IP mancante.")
                self.payload = "Errore: tipo o indirizzo IP mancante."
                return self

            self.register_dongle(dongle_type, ip_address, payload)

            if dongle_type == "actuator":
                self.send_activation_message(ip_address)

            self.payload = f"Registrazione completata per il donglee di tipo {dongle_type}."
            
            return self

        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            return self
        except Error as e:
            self.payload = f"Errore durante la registrazione del donglee: {e}"
            return self

    def fetch_dongle_from_db(self, type):
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            select_dongle_query = """
            SELECT ip_address, type
            FROM dongle
            WHERE type = %s
            """
            cursor.execute(select_dongle_query, (type,))
            dongle_data = cursor.fetchall()

            dongle = {}
            if dongle_data:
                for row in dongle_data:
                    ip_address, type = row
                    dongle[type] = {"ip_address": ip_address}
                    response = {
                        "dongle": type,
                        "ip_address": dongle[type]["ip_address"]
                    }
                    self.payload = json.dumps(response, separators=(',', ':'))
                    print(f"Payload: {self.payload}")
            else:
                self.payload = None
                print(f"No dongle found for type: {type}. Payload: {self.payload}")
        except Error as e:
            self.payload = None
            print(f"Error retrieving dongle data: {e}, Payload: {self.payload}")
        finally:
            if cursor:
                cursor.close()

    def register_dongle(self, dongle_type, ip_address, payload):
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            INSERT INTO dongle (type, ip_address)
            VALUES (%s, %s)
            """
            cursor.execute(query, (str(dongle_type), str(ip_address)))
            self.connection.commit()
            print(f"Registrato il donglee di tipo {dongle_type} con indirizzo IP {ip_address}.")

            if dongle_type == "actuator":
                query = """
                INSERT INTO dispositivi (ip_address, nome, consumo_kwh, durata)
                VALUES (%s, %s, %s, %s)
                """
                name = payload.get("n")
                consumo_kwh = payload.get("c")
                durata = payload.get("d")
                if not name or not consumo_kwh or not durata:
                    print(f"Errore: Dati mancanti nel payload. name={name}, consumo_kwh={consumo_kwh}, durata={durata}")
                cursor.execute(query, (str(ip_address), str(name), float(consumo_kwh), int(durata)))
                self.connection.commit()
                print(f"Registrato il dispositivo di tipo {dongle_type} con indirizzo IP {ip_address}.")
            if dongle_type in ["solar", "power"]:
                self.notify_actuators()
        except Error as e:
            print(f"Errore durante la registrazione del donglee: {e}")
        finally:
            if cursor:
                cursor.close()

    def notify_actuators(self):
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            SELECT ip_address FROM dongle
            WHERE type = 'actuator'
            """
            cursor.execute(query)
            actuators = cursor.fetchall()

            for actuator in actuators:
                actuator_ip = actuator[0]
                self.send_activation_message(actuator_ip)
        except Error as e:
            print(f"Errore durante la notifica degli attuatori: {e}")
        finally:
            if cursor:
                cursor.close()

    def send_activation_message(self, ip_address):
        client = None
        cursor = None
        try:
            if isinstance(ip_address, str):
                ip_address = eval(ip_address)

            if isinstance(ip_address, tuple) and len(ip_address) == 2:
                ip, port = ip_address
            else:
                raise ValueError(f"Formato non valido per ip_address: {ip_address}")

            print(f"Invio messaggio di attivazione al dispositivo con IP: {ip} e porta: {port}")

            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            SELECT type, ip_address FROM dongle
            WHERE type IN ('solar', 'power')
            """
            cursor.execute(query)
            dongles = cursor.fetchall()

            solar_ip = ""
            power_ip = ""

            for dongle_type, ip_port in dongles:
                if isinstance(ip_port, str):
                    ip_port = eval(ip_port)
                if isinstance(ip_port, tuple) and len(ip_port) == 2:
                    dongle_ip, dongle_port = ip_port
                    formatted_ip = f"coap://[{dongle_ip}]:{dongle_port}"
                    if dongle_type == "solar":
                        solar_ip = formatted_ip
                        print(f"Solar IP: {solar_ip}")
                    elif dongle_type == "power":
                        power_ip = formatted_ip
                        print(f"Power IP: {power_ip}")

            print(f"INVIO DEGLI INDIRIZZI: Solar: {solar_ip}, Power: {power_ip}")

            client = HelperClient(server=(ip, port))
            payload = {
                "o": datetime.now().hour,
                "m": datetime.now().minute,
                "g": datetime.now().day,
                "h": datetime.now().month,
                "s": solar_ip,
                "p": power_ip
            }
            payload_json = json.dumps(payload) + "/end/"
            print(f"Payload con /end/: {payload_json}")

            payload_bytes = payload_json.encode('utf-8')
            block_size = 64
            blocks = [payload_bytes[i:i + block_size] for i in range(0, len(payload_bytes), block_size)]

            for i, block in enumerate(blocks):
                print(f"Inviando blocco {i + 1}/{len(blocks)}: {block.decode('utf-8', errors='ignore')}")
                response = client.post("smartplug", block, timeout=10)
                if response:
                    print(f"Blocco {i + 1} inviato con successo.")
                else:
                    print(f"Errore durante l'invio del blocco {i + 1}.")
                    break
        except Exception as e:
            print(f"Errore durante l'invio del messaggio CoAP: {e}")
        finally:
            if cursor:
                cursor.close()
            if client:
                client.stop()


