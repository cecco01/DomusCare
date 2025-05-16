from serverCoAP import CoAPServer
from coapthon.client.helperclient import HelperClient
import json

SERVER_IP = "fd00::1"
SERVER_PORT = 5683

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

def cambia_stato_dispositivo(nome, nuovo_stato, ore=0):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        payload = {
            "nome": nome,
            "stato": nuovo_stato
        }
        response = client.post("control/", json.dumps(payload))
        if response and response.code == 68:
            print(f"Stato del dispositivo '{nome}' aggiornato con successo sul server.")
        else:
            print(f"Errore nell'aggiornamento dello stato del dispositivo sul server: {response.code if response else 'Nessuna risposta'}")
        actuator_ip = recupera_ip_attuatore(nome)
        if actuator_ip:
            invia_richiesta_attuatore(actuator_ip, nuovo_stato, ore)
        else:
            print(f"Errore: impossibile trovare l'indirizzo IP dell'attuatore '{nome}'.")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

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

def invia_richiesta_attuatore(ip_address, nuovo_stato, ore):
    client = HelperClient(server=(ip_address, SERVER_PORT))
    try:
        payload = {
            "stato": nuovo_stato,
            "tempo_limite": ore
        }
        response = client.post("stato/", json.dumps(payload))
        if response and response.code == 68:
            print(f"Stato dell'attuatore '{ip_address}' aggiornato con successo.")
        else:
            print(f"Errore nell'aggiornamento dello stato dell'attuatore: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP all'attuatore: {e}")
    finally:
        client.stop()

def rimuovi_dispositivo(nome):
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        payload = {"nome": nome}
        response = client.delete("control/", json.dumps(payload))
        if response and response.code == 68:
            print(f"Dispositivo '{nome}' rimosso con successo.")
        else:
            print(f"Errore nella rimozione del dispositivo: {response.code if response else 'Nessuna risposta'}")
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
    finally:
        client.stop()

def recupera_lista_attuatori():
    client = HelperClient(server=(SERVER_IP, SERVER_PORT))
    try:
        response = client.get("control/?type=actuator")
        if response:
            attuatori = json.loads(response.payload)
            print("Attuatori disponibili:")
            for attuatore in attuatori:
                print(f"Nome: {attuatore['nome']}, IP: {attuatore['ip_address']}")
            return attuatori
        else:
            print("Errore nel recupero della lista attuatori.")
            return []
    except Exception as e:
        print(f"Errore nella richiesta CoAP: {e}")
        return []
    finally:
        client.stop()

def remote_cli():
    print("Recupero lista attuatori dal server...")
    recupera_lista_attuatori()
    while True:
        print("\nGestione da terminale:")
        print("1. Mostra dispositivi")
        print("2. Cambia stato dispositivo")
        print("3. Rimuovi dispositivo")
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
            nome = input("Inserisci il nome del dispositivo da rimuovere: ")
            rimuovi_dispositivo(nome)
        elif scelta == "0":
            print("Uscita...")
            break
        else:
            print("Scelta non valida.")


def main():
    print("Scegli modalità di avvio:")
    print("1. Avvia server CoAP")
    print("2. Gestione remota (CLI)")
    scelta = input("Scelta: ")
    if scelta == "1":
        host = "::"
        port = 5683
        server = CoAPServer(host, port)
        try:
            print("CoAP server start")
            server.listen(10)
        except KeyboardInterrupt:
            print("Server Shutdown")
            server.close()
            print("Exiting...")
    elif scelta == "2":
        remote_cli()
    else:
        print("Scelta non valida.")

if __name__ == "__main__":
    main()