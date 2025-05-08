import asyncio
from aiocoap import *
import mysql.connector
from mysql.connector import Error

DB_HOST = "localhost"
DB_USER = "Luca"
DB_PASS = "Luca"
DB_NAME = "iot"

# Funzione per connettersi al database MySQL
def connect_db():
    try:
        connection = mysql.connector.connect(
            host=DB_HOST,
            user=DB_USER,
            password=DB_PASS,
            database=DB_NAME
        )
        return connection
    except Error as e:
        print(f"Errore nella connessione al database: {e}")
        return None

# Funzione per gestire i messaggi ricevuti
async def handle_request(request):
    payload = request.payload.decode('utf-8')
    print(f"Messaggio ricevuto: {payload}")

    # Connessione al database
    connection = connect_db()
    if not connection:
        return Message(code=INTERNAL_SERVER_ERROR, payload=b"Errore nella connessione al database")

    try:
        # Parsing del payload (esempio: {"nome": "Dispositivo1", "stato": 2, "tempo_limite": 4})
        import json
        data = json.loads(payload)
        nome = data.get("nome")
        stato = data.get("stato", 0)
        tempo_limite = data.get("tempo_limite", 0)

        # Aggiorna lo stato del dispositivo nel database
        cursor = connection.cursor()
        if stato == 2:  # Stato "Pronto"
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NOW() + INTERVAL %s HOUR
            WHERE nome = %s
            """
            cursor.execute(query, (stato, tempo_limite, nome))
        else:  # Altri stati
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NULL
            WHERE nome = %s
            """
            cursor.execute(query, (stato, nome))

        connection.commit()
        print(f"Stato del dispositivo '{nome}' aggiornato a {stato} con tempo limite {tempo_limite} ore.")
        return Message(code=CHANGED, payload=b"Stato aggiornato con successo")
    except Exception as e:
        print(f"Errore durante la gestione del messaggio: {e}")
        return Message(code=INTERNAL_SERVER_ERROR, payload=b"Errore durante la gestione del messaggio")
    finally:
        connection.close()

# Risorsa CoAP per gestire le richieste
class CoAPResource:
    async def render_post(self, request):
        return await handle_request(request)

# Configurazione del server CoAP
async def main():
    root = resource.Site()
    root.add_resource(('gestione',), CoAPResource())

    # Avvia il server CoAP
    await Context.create_server_context(root)
    print("Server CoAP in esecuzione...")
    await asyncio.get_running_loop().create_future()  # Mantiene il server attivo

if __name__ == "__main__":
    asyncio.run(main())