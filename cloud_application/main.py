from serverCoAP import CoAPServer


import subprocess

def reset_database():
    # Modifica questi parametri secondo le tue esigenze
    user = "root"
    password = "root"
    db_name = "iot"
    sql_file = "/media/sf_AirClean/database/db.sql"

    # Costruisci il comando
    command = [
        "mysql",
        f"-u{user}",
        f"-p{password}",
        db_name
    ]

    # Apri il file SQL e passalo come input al comando
    with open(sql_file, "r") as sql:
        result = subprocess.run(command, stdin=sql)
        if result.returncode == 0:
            print("Database resettato.")
        else:
            print("Errore nel reset del database.")


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