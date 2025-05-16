from serverCoAP import CoAPServer

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
            server.listen(10) # 10 è il numero di pacchetti in attesa
        except KeyboardInterrupt:
            print("Server Shutdown")
            server.close()
            print("Exiting...")
    elif scelta == "2":
        from cli import remote_cli
        remote_cli()
    else:
        print("Scelta non valida.")

if __name__ == "__main__":
    main()