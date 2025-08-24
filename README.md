# DomusCare  
🧑‍💻 Leonardo Ceccarelli, Luca Rotelli  
📅 A.A. 2024/2025  

---

## 📋 Project Overview  
DomusCare è un sistema intelligente di **Smart Home Management** basato su IoT, progettato per ottimizzare i consumi energetici domestici.  
L’architettura integra sensori per il monitoraggio della produzione solare e dei consumi elettrici, insieme ad attuatori (smart plug) che gestiscono l’attivazione degli elettrodomestici.  

L’obiettivo principale è **ridurre gli sprechi energetici**, migliorare l’efficienza nell’uso delle fonti rinnovabili e favorire stili di vita sostenibili attraverso un’infrastruttura leggera, scalabile e a basso consumo.  

---

## 🌟 Introduction  
Con la crescita delle abitazioni connesse, ottimizzare i consumi energetici rappresenta una delle sfide più rilevanti.  
DomusCare sfrutta una rete CoAP e tecniche di Machine Learning per:  

- Monitorare in tempo reale consumi e produzione solare.  
- Prevedere surplus o deficit energetici.  
- Pianificare in modo intelligente l’attivazione degli elettrodomestici.  
- Fornire alti livelli di automazione e sostenibilità.  

---

## ⚙️ Application Logic  
Il funzionamento del sistema si basa su:  

- **Sensori**:  
  - **Power Sensor**: misura i consumi elettrici degli elettrodomestici.  
  - **Solar Sensor**: rileva l’energia prodotta dai pannelli fotovoltaici.  

- **Attuatore (Smart Plug)**:  
  - Decide autonomamente quando attivare il dispositivo collegato, in base a previsioni di produzione/consumo.  
  - Gestisce fallback e retry in caso di condizioni non ottimali.  
  - Colori LED distintivi: blu (asciugatrice), rosso (lavastoviglie), giallo (lavatrice).  

- **Cloud Application**:  
  - Raccoglie i dati in un database MySQL.  
  - Fornisce un’interfaccia per osservare e controllare i dispositivi.  

---

## 🏗️ Architecture  

### 🌐 CoAP Network  
- Basata su **nRF52840 dongles**.  
- Sensori e attuatori comunicano via border router.  
- Ogni nodo si registra presso un **CoAP Registration Server**.  

### ⚡ Database  
Strutturato in più tabelle:  
- **data_solar** → produzione da pannelli solari.  
- **data_power** → consumi domestici.  
- **dongle** → IP e tipologia dei nodi.  
- **devices** → stato e caratteristiche degli elettrodomestici.  

### 🔧 Data Encoding  
- Payload in **JSON**, per ridurre la complessità e migliorare leggibilità.  

---

## 🚀 Deployment and Execution  

### 🔑 Node Registration  
Ogni nodo invia al server tipo, IP e stato operativo.  

### 🔌 Sensors  
- `res-solar.c` → risorsa `/valore` con produzione solare.  
- `res-power.c` → risorsa `/valore` con consumo elettrico.  

### ⚡ Smart Plug Actuator  
- Server: espone `/res-smartplug` per eventi di attivazione.  
- Client: osserva sensori e pianifica attivazioni.  

### 🎛️ Remote Control Application (CLI)  
Comandi principali:  
- `Mostra dispositivi`  
- `Cambia stato dispositivo`  
- `Rimuovi dispositivo`  
- `Esci`  

---

## 📊 Grafana  
Il sistema integra **Grafana** per la visualizzazione dei dati in tempo reale:  
- Confronto tra produzione e consumo.  
- Analisi temporale dei picchi di energia.  

---

## 🤖 Machine Learning  
DomusCare utilizza modelli **TensorFlow Keras**, ottimizzati con **emlearn** per dispositivi embedded.  

- **Modello 1**: previsione produzione solare.  
- **Modello 2**: previsione consumo energetico.  

