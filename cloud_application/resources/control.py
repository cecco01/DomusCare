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

    def render_GET(self, request):
        """
        Gestisce le richieste GET per ottenere informazioni sui dispositivi o donglei.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con i dati richiesti.
        """
        #print(f"RENDER GET - Request parameters: {request.uri_query}")
        query = request.uri_query
        #aggiunta per far sì che, quando riceve una richiesta GET con la query string ?type=actuator, il server risponda con la lista degli attuatori e i loro indirizzi IP.
        # Se la query è 'type=actuator', restituisci la lista degli attuatori
        if query and "type=actuator" in query:
            try:
                self.ensure_connection()
                cursor = self.connection.cursor()
                # Adatta la query SQL ai nomi delle tue colonne/tabelle!
                cursor.execute("SELECT nome, ip_address FROM dispositivi WHERE tipo = 'actuator'")
                attuatori = [{"nome": row[0], "ip_address": row[1]} for row in cursor.fetchall()]
                cursor.close()
                self.payload = json.dumps(attuatori)
                #print(f"Payload attuatori: {self.payload}")
            except Error as e:
                self.payload = None
                print(f"Errore nel recupero attuatori: {e}")
                return self
        #fine aggiunta
        #altra correzione: prima prendeva solo il primo elemento della query string, ora prende tutto
        dongle_type = None
        if query and query.startswith("type="):
            dongle_type = query.split("=")[1]
        self.fetch_dongle_from_db(dongle_type)
        return self

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

    def render_PUT(self, request):
        """
        Gestisce le richieste PUT per modificare le informazioni di un dispositivo o donglee.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con il risultato dell'operazione.
        """
        try:
            payload = json.loads(request.payload)
            nome = payload.get("nome")
            nuovo_nome = payload.get("nuovo_nome")
            nuovo_tipo = payload.get("nuovo_tipo")

            if not nome or not nuovo_nome or not nuovo_tipo:
                self.payload = "Errore: parametri mancanti per l'aggiornamento."
                return self

            self.update_device_info(nome, nuovo_nome, nuovo_tipo)
            self.payload = f"Dispositivo '{nome}' aggiornato con successo."
            print(self.payload)
            return self
        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            print(self.payload)
            return self
        except Error as e:
            self.payload = f"Errore durante l'aggiornamento del dispositivo: {e}"
            print(self.payload)
            return self

    def render_DELETE(self, request):
        """
        Gestisce le richieste DELETE per rimuovere un dispositivo o donglee.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con il risultato dell'operazione.
        """
        try:
            payload = json.loads(request.payload)
            nome = payload.get("nome")

            if not nome:
                self.payload = "Errore: nome del dispositivo mancante."
                return self

            self.delete_device(nome)
            self.payload = f"Dispositivo '{nome}' rimosso con successo."
            print(self.payload)
            return self
        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            print(self.payload)
            return self
        except Error as e:
            self.payload = f"Errore durante la rimozione del dispositivo: {e}"
            print(self.payload)
            return self

    def fetch_dongle_from_db(self, dongle_type):
        """
        Recupera i dati di un donglee dal database.

        :param dongle_type: Tipo del donglee da cercare.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            SELECT ip_address, type
            FROM dongle
            WHERE type = %s
            """
            cursor.execute(query, (dongle_type,))
            dongle_data = cursor.fetchall()
            if dongle_data:
                dongle = {row[1]: {"ip_address": row[0]} for row in dongle_data}
                self.payload = json.dumps(dongle, separators=(',', ':'))
                print(f"Payload: {self.payload}")
            else:
                self.payload = None
                print(f"No dongle found for type: {dongle_type}. Payload: {self.payload}")
        except Error as e:
            self.payload = None
            print(f"Error retrieving dongle data: {e}, Payload: {self.payload}")
        finally:
            if cursor:
                cursor.close()

    def update_device_status(self, nome, stato, tempo_limite):
        """
        Aggiorna lo stato di un dispositivo nel database.

        :param nome: Nome del dispositivo.
        :param stato: Nuovo stato del dispositivo.
        :param tempo_limite: Tempo limite per il completamento del task.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NOW() + INTERVAL %s HOUR
            WHERE nome = %s
            """
            cursor.execute(query, (stato, tempo_limite, nome))
            self.connection.commit()
            print(f"Stato del dispositivo '{nome}' aggiornato a {stato} con tempo limite {tempo_limite} ore.")
        except Error as e:
            print(f"Errore durante l'aggiornamento dello stato del dispositivo: {e}")
        finally:
            if cursor:
                cursor.close()

    def update_device_info(self, nome, nuovo_nome, nuovo_tipo):
        """
        Aggiorna le informazioni di un dispositivo nel database.

        :param nome: Nome attuale del dispositivo.
        :param nuovo_nome: Nuovo nome del dispositivo.
        :param nuovo_tipo: Nuovo tipo del dispositivo.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = """
            UPDATE dispositivi
            SET nome = %s, tipo = %s
            WHERE nome = %s
            """
            cursor.execute(query, (nuovo_nome, nuovo_tipo, nome))
            self.connection.commit()
            print(f"Dispositivo '{nome}' aggiornato a '{nuovo_nome}' con tipo '{nuovo_tipo}'.")
        except Error as e:
            print(f"Errore durante l'aggiornamento del dispositivo: {e}")
        finally:
            if cursor:
                cursor.close()

    def delete_device(self, nome):
        """
        Rimuove un dispositivo dal database.

        :param nome: Nome del dispositivo da rimuovere.
        """
        cursor = None
        try:
            self.ensure_connection()
            cursor = self.connection.cursor()
            query = "DELETE FROM dispositivi WHERE nome = %s"
            cursor.execute(query, (nome,))
            self.connection.commit()
            print(f"Dispositivo '{nome}' rimosso con successo.")
        except Error as e:
            print(f"Errore durante la rimozione del dispositivo: {e}")
        finally:
            if cursor:
                cursor.close()

