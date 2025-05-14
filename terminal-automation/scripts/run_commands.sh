#!/bin/bash

# Open a new terminal and run the Gradle command with the specific simulation
gnome-terminal -- bash -c "cd ~/contiki-ng/tools/cooja && ./gradlew run --args='--cooja=/home/iot_ubuntu_intel/simulazione_4.csc' --stacktrace --info --debug; exec bash"

# Open a new terminal and run the Python application
gnome-terminal -- bash -c "python3 /media/sf_AirClean/cloud_application/main.py; exec bash"

# Open a new terminal and compile the RPL border router
gnome-terminal -- bash -c "cd /media/sf_AirClean/rpl-border-router && make TARGET=cooja connect-router-cooja; exec bash"

# Open a new terminal and execute the MySQL command
gnome-terminal -- bash -c "mysql -u root -p -e 'SOURCE /media/sf_AirClean/database/db.sql'; exec bash"

echo "Tutti i comandi sono stati avviati in terminali separati."