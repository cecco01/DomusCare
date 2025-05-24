from mysql.connector import Error
from coapthon.resources.resource import Resource
from core.database import Database
from coapthon.client.helperclient import HelperClient
import json
from coapthon import defines
import time
import re

class Control(Resource):
    def __init__(self, name="Control"):
        super(Control, self).__init__(name)
        self.payload = "Control Resource"
        self.database = Database()
        self.connection = self.database.connect_db()

    def ensure_connection(self):
        if not self.connection.is_connected():
            print("Connessione al database persa. Riconnessione in corso...")
            self.database = Database()
            self.connection = self.database.connect_db()
            if not self.connection.is_connected():
                raise Error("Impossibile riconnettersi al database.")

    
    def render_POST(self, request):
        """
        Gestisce le richieste POST per aggiornare lo stato di un dispositivo o donglee.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con il risultato dell'operazione.
        """
        print(f"CONTROL: RENDER POST ")
        print(f"Payload ricevuto grezzo: {request.payload}")
        # Fix: sostituisci la virgola con il punto solo nei numeri
        try:
            # Parsing del payload JSON
            payload = json.loads(request.payload)
            print(f"payload ricevuto:{payload}")
            tipo = payload.get("t")

            # Gestione dei valori in base al tipo
            if tipo == "solar":
                valore = payload.get("value")/100
                solarpower = valore
                power = None
                smartplug = None
                self.insert_data(solarpower, power)
            elif tipo == "power":
                valore = payload.get("value")/100
                solarpower = None
                power = valore
                smartplug = None
                self.insert_data(solarpower, power)
            elif tipo == "actuator":
                solarpower = None
                power = None
                stato = payload.get("stato")
                print(f"stato: {stato}")
                #prendo l'ip_address dal payload
                ip_address = request.source
                print(f"ip_address: {ip_address},payload: {payload}")             
                self.cambia_stato(ip_address, stato)
            else:
                self.payload = "Errore: tipo non riconosciuto."
                print(self.payload)
                return self
            
            return self
        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            print(self.payload)
            return self
        except Error as e:
            self.payload = f"Errore durante l'inserimento dei dati: {e}"
            print(self.payload)
            return self

    def cambia_stato(self, ip, stato):
        """
        Cambia lo stato di un dispositivo nel database.
        """
        print(f"CONTROL: cambia_stato")
        cursor = None
        try:
            self.ensure_connection()
            print(f"CONTROL: cambia_stato connessione al db effettuata")
            nome = self.get_nome_by_ip(ip)
            print(f"CONTROL: cambia_stato nome del dispositivo: {nome}")
            if not nome:
                print(f"Errore: nessun dispositivo trovato con l'indirizzo IP {ip}.")
                return

            cursor = self.connection.cursor()
            query = """
            UPDATE dispositivi
            SET stato = %s
            WHERE nome = %s
            """
            cursor.execute(query, (stato, nome))
            self.connection.commit()
            print(f"Stato del dispositivo '{nome}' aggiornato a {stato}.")
        except Error as e:
            print(f"Errore durante l'aggiornamento dello stato del dispositivo: {e}")
        finally:
            if cursor:
                cursor.close()

    def get_nome_by_ip(self, ip):
        """
        Recupera il nome di un dispositivo in base al suo indirizzo IP.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            ip = str(ip)
            query = "SELECT nome FROM dispositivi WHERE ip_address = %s"
            cursor.execute(query, (ip,))
            result = cursor.fetchone()
            if result:
                return result[0]
            else:
                print(f"Nessun dispositivo trovato con l'IP {ip}.")
                return None
        except Error as e:
            print(f"Errore durante il recupero del nome del dispositivo: {e}")
        finally:
            if cursor:
                cursor.close()

    def insert_data(self, solarpower, power):
        """
        Inserisce un nuovo record nella tabella 'data'.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = "INSERT INTO data (solarpower, power) VALUES (%s, %s)"
            cursor.execute(query, (solarpower, power))
            self.connection.commit()
            #print(f"Nuovo record inserito nella tabella 'data': solarpower={solarpower}, power={power}")
        except Error as e:
            print(f"Errore durante l'inserimento dei dati nella tabella 'data': {e}")
        finally:
            if cursor:
                cursor.close()
