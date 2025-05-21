from serverCoAP import CoAPServer
import subprocess
import json

def reset_database():
    # Carica le credenziali dal file JSON
    with open("private/credentials.json", "r") as file:
        credentials = json.load(file)
    user = credentials["MYSQL_USER"]
    password = credentials["MYSQL_PASSWORD"]
    db_name = credentials["MYSQL_DATABASE"]

    # Comandi SQL per svuotare solo le tabelle desiderate
    sql_commands = """
    TRUNCATE TABLE dongle;
    TRUNCATE TABLE dispositivi;
    """

    command = [
        "mysql",
        f"-u{user}",
        f"-p{password}",
        db_name
    ]

    result = subprocess.run(command, input=sql_commands.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode == 0:
        print("Tabelle dongle e dispositivi resettate.")
    else:
        print("Errore nel reset delle tabelle:", result.stderr.decode())

def main():
    reset_database()
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

if __name__ == "__main__":
    main()