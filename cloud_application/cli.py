from coapthon.client.helperclient import HelperClient
import json
from core.database import Database

def get_ip_by_name(nome):
    """
    Recupera l'indirizzo IP di un dispositivo dato il suo nome.
    """
    try:
        # Ottieni la connessione al database tramite la classe Database
        db = Database()
        conn = db.connect_db()
        if conn is None:
            print("Errore: impossibile connettersi al database.")
            return None

        cursor = conn.cursor(dictionary=True)

        # Esegui la query per ottenere l'indirizzo IP
        query = "SELECT ip_address FROM dispositivi WHERE nome = %s"
        cursor.execute(query, (nome,))
        result = cursor.fetchone()

        # Chiudi il cursore
        cursor.close()

        if result:
            return result["ip_address"]
        else:
            print(f"Errore: nessun dispositivo trovato con il nome '{nome}'.")
            return None
    except Exception as e:
        print(f"Errore durante il recupero dell'indirizzo IP: {e}")
        return None

def cambia_stato_dispositivo(nome, nuovo_stato, ore=0):
    """
    Cambia lo stato di un dispositivo dato il suo nome.
    """
    # Ottieni l'indirizzo IP del dispositivo
    ip_address = get_ip_by_name(nome)
    if isinstance(ip_address, str):
        ip_address = eval(ip_address)  # Converte la stringa in una tupla

    # Estrai l'indirizzo IP e la porta dalla tupla
    if isinstance(ip_address, tuple) and len(ip_address) == 2:
        ip, port = ip_address
    else:
        raise ValueError(f"Formato non valido per ip_address: {ip_address}")

    if ip_address is None:
        print(f"Impossibile cambiare lo stato del dispositivo '{nome}'.")
        return

    client = HelperClient(server=(ip,port ))
    try:
        if ore==0:
            payload = {
                "s": nuovo_stato
            }
        else:
            payload = {
                "s": nuovo_stato,
                "t":ore
            }
        
        #print(f"Payload inviato: {json.dumps(payload)} a : {ip}:{port}")
        response = client.post("remote_smartplug", json.dumps(payload))
        if response and response.code == 68:
            print(f"Stato del dispositivo '{nome}' aggiornato con successo sul server.")
        else:
            print(f"Errore nell'aggiornamento dello stato del dispositivo sul server: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

def rimuovi_dispositivo(nome):
   
    #rimuovi_dispositivo(nome):
    """
    Rimuove un dispositivo dal database.
    """
    try:
        # Ottieni la connessione al database tramite la classe Database
        db = Database()
        conn = db.connect_db()
        if conn is None:
            print("Errore: impossibile connettersi al database.")
            return

        cursor = conn.cursor()

        # Esegui la query per rimuovere il dispositivo
        query = "DELETE FROM dispositivi WHERE nome = %s"
        cursor.execute(query, (nome,))
        conn.commit()
        #rimuovo da sensor
        get_ip = get_ip_by_name(nome)
        
        query = "DELETE FROM sensor WHERE ip = %s"
        cursor.execute(query, (get_ip,))
        conn.commit()


        # Chiudi il cursore e la connessione
        cursor.close()
        conn.close()

        print(f"Dispositivo '{nome}' rimosso con successo dal database.")
    except Exception as e:
        print(f"Errore durante la rimozione del dispositivo: {e}")

def recupera_lista_attuatori():
    try:
        # Ottieni la connessione al database tramite la classe Database
        db = Database()
        conn = db.connect_db()
        if conn is None:
            print("Errore: impossibile connettersi al database.")
            return

        cursor = conn.cursor(dictionary=True)

        # Esegui la query per recuperare gli attuatori
        query = "SELECT nome, stato, consumo_kwh, durata FROM dispositivi"
        cursor.execute(query)
        dispositivi = cursor.fetchall()

        # Mostra i dispositivi in formato tabella
        print("+-----------------+-----------+--------------+-------------+")
        print("| {:15} | {:9} | {:12} | {:11} |".format("Nome", "Stato", "Consumo kWh", "Durata(min)"))
        print("+-----------------+-----------+--------------+-------------+")
        for dispositivo in dispositivi:
            nome = dispositivo["nome"]
            stato = dispositivo["stato"]
            consumo = dispositivo["consumo_kwh"]
            durata = dispositivo["durata"]
            stato_str = "Attivo" if stato == 1 else "Inattivo" if stato == 0 else "Pronto"
            print("| {:15} | {:9} | {:12} | {:11} |".format(nome, stato_str, consumo, durata))
        print("+-----------------+-----------+--------------+-------------+")


        # Chiudi il cursore
        cursor.close()
    except Exception as e:
        print(f"Errore durante il recupero degli attuatori: {e}")

def remote_cli():    
    while True:
        print("\nGestione da terminale:")
        print("1. Mostra dispositivi")
        print("2. Cambia stato dispositivo")
        print("3. Rimuovi dispositivo")
        print("0. Esci")
        scelta = input().strip()  # Rimuove eventuali spazi o caratteri di nuova riga

        if not scelta:  # Controlla se l'input è vuoto
            continue  # Ignora l'input vuoto e ripeti il ciclo

        if scelta == "1":
            recupera_lista_attuatori()
        elif scelta == "2":
            nome = input("Inserisci il nome del dispositivo: ").strip()
            nuovo_stato = int(input("Inserisci il nuovo stato (2=Pronto, 1=Attivo, 0=Inattivo): ").strip())
            if nuovo_stato == 2:
                ore = int(input("Inserisci entro quante ore deve essere completata la task : ").strip()) if nuovo_stato == 2 else 0
                cambia_stato_dispositivo(nome, nuovo_stato, ore)
            else:
                cambia_stato_dispositivo(nome, nuovo_stato, 0)
        elif scelta == "3":
            nome = input("Inserisci il nome del dispositivo da rimuovere: ").strip()
            rimuovi_dispositivo(nome)
        elif scelta == "0":
            print("Uscita...")
            break
        else:
            print("Scelta non valida.")

if __name__ == "__main__":
    remote_cli()