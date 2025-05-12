from coapthon.client.helperclient import HelperClient
import json

# Configurazione del server CoAP
SERVER_IP = "fd00::1"  # Modifica con l'indirizzo IP corretto del server
SERVER_PORT = 5683

# Funzione per mostrare i dispositivi
def mostra_dispositivi():
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        response = client.get("control/")
        if response:
            dispositivi = json.loads(response.payload)
            print("Dispositivi:")
            for dispositivo in dispositivi:
                nome = dispositivo["nome"]
                tipo = dispositivo["tipo"]
                stato = dispositivo["stato"]
                stato_str = "Attivo" if stato == 1 else "Inattivo" if stato == 0 else "Pronto"
                print(f"Nome: {nome}, Tipo: {tipo}, Stato: {stato_str}")
        else:
            print("Errore nel recupero dei dispositivi.")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

# Funzione per inviare una richiesta CoAP al server per cambiare lo stato di un dispositivo
def cambia_stato_dispositivo(nome, nuovo_stato, ore=0):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        # Invia la richiesta al server CoAP
        payload = {
            "nome": nome,
            "stato": nuovo_stato
        }
        response = client.post("control/", json.dumps(payload))
        if response and response.code == 68:  # ACK
            print(f"Stato del dispositivo '{nome}' aggiornato con successo sul server.")
        else:
            print(f"Errore nell'aggiornamento dello stato del dispositivo sul server: {response.code if response else 'Nessuna risposta'}")

        # Recupera l'indirizzo IP dell'attuatore dal server
        actuator_ip = recupera_ip_attuatore(nome)
        if actuator_ip:
            # Invia la richiesta CoAP all'attuatore
            invia_richiesta_attuatore(actuator_ip, nuovo_stato, ore)
        else:
            print(f"Errore: impossibile trovare l'indirizzo IP dell'attuatore '{nome}'.")

    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

# Funzione per recuperare l'indirizzo IP di un attuatore dal server
def recupera_ip_attuatore(nome):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        response = client.get(f"control/?device={nome}")
        if response:
            dispositivo = json.loads(response.payload)
            return dispositivo.get("ip_address")
        else:
            print(f"Errore nel recupero dell'indirizzo IP per il dispositivo '{nome}'.")
            return None
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
        return None
    finally:
        client.stop()

# Funzione per inviare una richiesta CoAP all'attuatore
def invia_richiesta_attuatore(ip_address, nuovo_stato, ore):
    client = HelperClient(server=(ip_address, SERVER_PORT))
    try:
        payload = {
            "stato": nuovo_stato,
            "tempo_limite": ore
        }
        response = client.post("stato/", json.dumps(payload))
        if response and response.code == 68:  # ACK
            print(f"Stato dell'attuatore '{ip_address}' aggiornato con successo.")
        else:
            print(f"Errore nell'aggiornamento dello stato dell'attuatore: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP all'attuatore: {e}")
    finally:
        client.stop()

# Funzione per inserire un nuovo dispositivo
def inserisci_dispositivo(nome, tipo, consumo_kwh, durata):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        payload = {
            "nome": nome,
            "tipo": tipo,
            "consumo_kwh": consumo_kwh,
            "durata": durata
        }
        response = client.post("register/", json.dumps(payload))
        if response and response.code == 68:  # ACK
            print(f"Dispositivo '{nome}' inserito con successo.")
        else:
            print(f"Errore nell'inserimento del dispositivo: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

# Funzione per rimuovere un dispositivo
def rimuovi_dispositivo(nome):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        payload = {"nome": nome}
        response = client.delete("control/", json.dumps(payload))
        if response and response.code == 68:  # ACK
            print(f"Dispositivo '{nome}' rimosso con successo.")
        else:
            print(f"Errore nella rimozione del dispositivo: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

# Menu principale
def main():
    while True:
        print("\nGestione da terminale:")
        print("1. Mostra dispositivi")
        print("2. Cambia stato dispositivo")
        print("3. Inserisci nuovo dispositivo")
        print("4. Rimuovi dispositivo")
        print("0. Esci")
        scelta = input()

        if scelta == "1":
            mostra_dispositivi()
        elif scelta == "2":
            nome = input("Inserisci il nome del dispositivo: ")
            nuovo_stato = int(input("Inserisci il nuovo stato (2=Pronto, 1=Attivo, 0=Inattivo): "))
            ore = int(input("Inserisci entro quante ore deve essere completata la task (se applicabile): ")) if nuovo_stato == 2 else 0
            cambia_stato_dispositivo(nome, nuovo_stato, ore)
        elif scelta == "3":
            nome = input("Inserisci il nome del dispositivo: ")
            tipo = input("Inserisci il tipo del dispositivo: ")
            consumo_kwh = float(input("Inserisci il consumo in kWh: "))
            durata = int(input("Inserisci la durata in minuti: "))
            inserisci_dispositivo(nome, tipo, consumo_kwh, durata)
        elif scelta == "4":
            nome = input("Inserisci il nome del dispositivo da rimuovere: ")
            rimuovi_dispositivo(nome)
        elif scelta == "0":
            print("Uscita...")
            break
        else:
            print("Scelta non valida.")

if __name__ == "__main__":
    main()