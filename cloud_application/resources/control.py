from mysql.connector import Error
from coapthon.resources.resource import Resource
from resources.database import Database
from coapthon.client.helperclient import HelperClient
import json
from coapthon import defines

class Control(Resource):
    def __init__(self, name="Control"):
        super(Control, self).__init__(name)
        self.payload = "Control Resource"
        self.database = Database()
        self.connection = self.database.connect_db()

    def render_GET(self, request):
        """
        Gestisce le richieste GET per ottenere informazioni sui dispositivi o sensori.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con i dati richiesti.
        """
        print(f"RENDER GET - Request parameters: {request.uri_query}")
        query = request.uri_query
        self.fetch_sensor_from_db(query)
        return self

    def render_POST(self, request):
        """
        Gestisce le richieste POST per aggiornare lo stato di un dispositivo o sensore.

        :param request: Richiesta CoAP ricevuta.
        :return: Risposta CoAP con il risultato dell'operazione.
        """
        try:
            payload = json.loads(request.payload)
            nome = payload.get("nome")
            stato = payload.get("stato", 0)

            if not nome:
                self.payload = "Errore: nome del dispositivo mancante."
                return self

            self.update_device_status(nome, stato, tempo_limite)
            self.payload = f"Stato del dispositivo '{nome}' aggiornato a {stato}."
            print(self.payload)
            return self
        except json.JSONDecodeError:
            self.payload = "Errore: payload JSON non valido."
            print(self.payload)
            return self
        except Error as e:
            self.payload = f"Errore durante l'aggiornamento dello stato del dispositivo: {e}"
            print(self.payload)
            return self

    def render_PUT(self, request):
        """
        Gestisce le richieste PUT per modificare le informazioni di un dispositivo o sensore.

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
        Gestisce le richieste DELETE per rimuovere un dispositivo o sensore.

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

    def fetch_sensor_from_db(self, sensor_type):
        """
        Recupera i dati di un sensore dal database.

        :param sensor_type: Tipo del sensore da cercare.
        """
        if not self.connection.is_connected():
            self.payload = None
            print("Database connection lost, Payload: None")
            return

        try:
            cursor = self.connection.cursor()
            query = """
            SELECT ip_address, type, status
            FROM sensor
            WHERE type = %s
            """
            cursor.execute(query, (sensor_type,))
            sensor_data = cursor.fetchall()
            cursor.close()

            if sensor_data:
                sensor = {row[1]: {"status": int(row[2]), "ip_address": row[0]} for row in sensor_data}
                self.payload = json.dumps(sensor, separators=(',', ':'))
                print(f"Payload: {self.payload}")
            else:
                self.payload = None
                print(f"No sensor found for type: {sensor_type}. Payload: {self.payload}")
        except Error as e:
            self.payload = None
            print(f"Error retrieving sensor data: {e}, Payload: {self.payload}")

    def update_device_status(self, nome, stato, tempo_limite):
        """
        Aggiorna lo stato di un dispositivo nel database.

        :param nome: Nome del dispositivo.
        :param stato: Nuovo stato del dispositivo.
        :param tempo_limite: Tempo limite per il completamento del task.
        """
        if not self.connection.is_connected():
            print("Database connection lost.")
            return

        try:
            cursor = self.connection.cursor()
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NOW() + INTERVAL %s HOUR
            WHERE nome = %s
            """
            cursor.execute(query, (stato, tempo_limite, nome))
            self.connection.commit()
            cursor.close()
            print(f"Stato del dispositivo '{nome}' aggiornato a {stato} con tempo limite {tempo_limite} ore.")
        except Error as e:
            print(f"Errore durante l'aggiornamento dello stato del dispositivo: {e}")

    def update_device_info(self, nome, nuovo_nome, nuovo_tipo):
        """
        Aggiorna le informazioni di un dispositivo nel database.

        :param nome: Nome attuale del dispositivo.
        :param nuovo_nome: Nuovo nome del dispositivo.
        :param nuovo_tipo: Nuovo tipo del dispositivo.
        """
        if not self.connection.is_connected():
            print("Database connection lost.")
            return

        try:
            cursor = self.connection.cursor()
            query = """
            UPDATE dispositivi
            SET nome = %s, tipo = %s
            WHERE nome = %s
            """
            cursor.execute(query, (nuovo_nome, nuovo_tipo, nome))
            self.connection.commit()
            cursor.close()
            print(f"Dispositivo '{nome}' aggiornato a '{nuovo_nome}' con tipo '{nuovo_tipo}'.")
        except Error as e:
            print(f"Errore durante l'aggiornamento del dispositivo: {e}")

    def delete_device(self, nome):
        """
        Rimuove un dispositivo dal database.

        :param nome: Nome del dispositivo da rimuovere.
        """
        if not self.connection.is_connected():
            print("Database connection lost.")
            return

        try:
            cursor = self.connection.cursor()
            query = "DELETE FROM dispositivi WHERE nome = %s"
            cursor.execute(query, (nome,))
            self.connection.commit()
            cursor.close()
            print(f"Dispositivo '{nome}' rimosso con successo.")
        except Error as e:
            print(f"Errore durante la rimozione del dispositivo: {e}")

