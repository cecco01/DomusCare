import mysql.connector
from mysql.connector import Error
from coapthon.client.helperclient import HelperClient

DB_HOST = "localhost"
DB_USER = "Luca"
DB_PASS = "Luca"
DB_NAME = "iot"

# Funzione per connettersi al database
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

# Funzione per verificare se un dispositivo esiste
def dispositivo_esiste(connection, nome):
    try:
        cursor = connection.cursor()
        query = "SELECT COUNT(*) FROM dispositivi WHERE nome = %s"
        cursor.execute(query, (nome,))
        count = cursor.fetchone()[0]
        cursor.close()
        return count > 0
    except Error as e:
        print(f"Errore nella query: {e}")
        return False

# Funzione per mostrare i dispositivi
def mostra_dispositivi(connection):
    try:
        cursor = connection.cursor()
        query = "SELECT nome, tipo, stato FROM dispositivi"
        cursor.execute(query)
        dispositivi = cursor.fetchall()
        cursor.close()

        print("Dispositivi:")
        for dispositivo in dispositivi:
            nome, tipo, stato = dispositivo
            stato_str = "Attivo" if stato else "Inattivo"
            print(f"Nome: {nome}, Tipo: {tipo}, Stato: {stato_str}")
    except Error as e:
        print(f"Errore nella query: {e}")

# Funzione per inviare una richiesta CoAP al server
def invia_richiesta_coap(nome_dispositivo, tempo_limite, consumo, produzione):
    ip_address = "fd00::1"  # Modifica con l'indirizzo IP corretto del server
    port = 5683
    client = HelperClient(server=(ip_address, port))

    try:
        # Creazione del payload con tutte le informazioni richieste
        payload = f'{{"nome": "{nome_dispositivo}", "tempo_limite": {tempo_limite}, "consumo": {consumo}, "produzione": {produzione}}}'
        print(f"Inviando richiesta CoAP al server con payload: {payload}")

        # Invio della richiesta POST al server
        response = client.post("gestione", payload)
        if response.code == 68:  # ACK
            print(f"Richiesta CoAP inviata con successo: {payload}")
        else:
            print(f"Errore nell'invio della richiesta CoAP: {response.code}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

# Funzione per cambiare lo stato di un dispositivo
def cambia_stato_dispositivo(connection, nome, nuovo_stato, ore):
    if not dispositivo_esiste(connection, nome):
        print(f"Errore: Il dispositivo '{nome}' non esiste nel database.")
        return

    try:
        cursor = connection.cursor()
        query = "SELECT stato, consumo_kwh, produzione_kwh FROM dispositivi WHERE nome = %s"
        cursor.execute(query, (nome,))
        result = cursor.fetchone()

        if result is None:
            print(f"Errore: Il dispositivo '{nome}' non esiste nel database.")
            return

        stato_attuale, consumo, produzione = result

        if stato_attuale != 0:
            print(f"Errore: Il dispositivo '{nome}' non è inattivo (stato attuale: {stato_attuale}).")
            return

        if nuovo_stato == 2:
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NOW() + INTERVAL %s HOUR
            WHERE nome = %s
            """
            cursor.execute(query, (nuovo_stato, ore, nome))
            connection.commit()

            # Invio della richiesta CoAP al server
            invia_richiesta_coap(nome, ore, consumo, produzione)
        else:
            query = """
            UPDATE dispositivi
            SET stato = %s, timestamp_attivazione = NULL
            WHERE nome = %s
            """
            cursor.execute(query, (nuovo_stato, nome))
            connection.commit()

        print(f"Stato del dispositivo '{nome}' aggiornato con successo.")
    except Error as e:
        print(f"Errore nell'aggiornamento dello stato: {e}")

# Funzione per inserire un nuovo dispositivo
def inserisci_dispositivo(connection):
    nome = input("Inserisci il nome del dispositivo: ")
    tipo = input("Inserisci il tipo del dispositivo: ")
    consumo_kwh = float(input("Inserisci il consumo in kWh: "))
    durata = int(input("Inserisci la durata in minuti: "))

    try:
        cursor = connection.cursor()
        query = """
        INSERT INTO dispositivi (nome, tipo, stato, consumo_kwh, durata)
        VALUES (%s, %s, 0, %s, %s)
        """
        cursor.execute(query, (nome, tipo, consumo_kwh, durata))
        connection.commit()
        print(f"Dispositivo '{nome}' inserito con successo.")
    except Error as e:
        print(f"Errore nell'inserimento del dispositivo: {e}")

# Funzione per rimuovere un dispositivo
def rimuovi_dispositivo(connection):
    nome = input("Inserisci il nome del dispositivo da rimuovere: ")

    if not dispositivo_esiste(connection, nome):
        print(f"Errore: Il dispositivo '{nome}' non esiste nel database.")
        return

    try:
        cursor = connection.cursor()
        query = "DELETE FROM dispositivi WHERE nome = %s"
        cursor.execute(query, (nome,))
        connection.commit()
        print(f"Dispositivo '{nome}' rimosso con successo.")
    except Error as e:
        print(f"Errore nella rimozione del dispositivo: {e}")

# Menu principale
def main():
    connection = connect_db()
    if connection is None:
        return

    while True:
        print("\nGestione da terminale:")
        print("1. Mostra dispositivi")
        print("2. Cambia stato dispositivo")
        print("3. Inserisci nuovo dispositivo")
        print("4. Rimuovi dispositivo")
        print("0. Esci")
        scelta = input()

        if scelta == "1":
            mostra_dispositivi(connection)
        elif scelta == "2":
            nome = input("Inserisci il nome del dispositivo: ")
            nuovo_stato = int(input("Inserisci il nuovo stato (2=Pronto, 1=Attivo, 0=Inattivo): "))
            if nuovo_stato == 2:
                ore = int(input("Inserisci entro quante ore deve essere completata la task: "))
            else:
                ore = 0
            cambia_stato_dispositivo(connection, nome, nuovo_stato, ore)
        elif scelta == "3":
            inserisci_dispositivo(connection)
        elif scelta == "4":
            rimuovi_dispositivo(connection)
        elif scelta == "0":
            print("Uscita...")
            break
        else:
            print("Scelta non valida.")

    connection.close()

if __name__ == "__main__":
    main()