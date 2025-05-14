import json
from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from coapthon.messages.request import Request
from coapthon.messages.response import Response
from coapthon.client.helperclient import HelperClient
from coapthon.utils import parse_uri
from core.database import Database
from core.record import Record
import re
#rivedi alcuni nomi delle risorse
class ObserveSensor:

    def __init__(self,source_address, resource):
        self.address = source_address
        self.resource = resource
        self.database = Database()
        Record.set_db(self.database)
        self.start_observing()
       
    def start_observing(self):
        self.client = HelperClient(self.address)
        self.client.observe(self.resource, self.observer)
    
    def observer(self, response):

        if not response.payload:
            print("Empty response payload")
            return
        
        data = json.loads(response.payload)
        
        try:
            
            if self.resource == "solarpower":
                print("\n🔄🔄🔄🔄 SOLAR POWER 🔄🔄🔄🔄 : " + str(data["value"]))
                Record.set_solarpower(data["value"])
            
            elif self.resource == "power":
                print("\n🔋🔋🔋🔋 POWER 🔋🔋🔋🔋 : " + str(data["value"]))
                Record.set_power(data["value"])

            else:
                print("Unknown resource:", self.resource)
        except KeyError as e:
            print("KeyError:", e)

class ObserveActuator:

    def __init__(self, source_address, resource):
        self.address = source_address
        self.resource = resource
        self.database = Database()
        self.start_observing()
       
    def start_observing(self):
        self.client = HelperClient(self.address)
        self.client.observe(self.resource, self.observer)
    
    def observer(self, response):
        if not response.payload:
            print("Empty response payload")
            return
        
        try:
            data = json.loads(response.payload)
            # Sostituisci "smartplug" con il nome reale della risorsa dell'attuatore
            if self.resource == "smartplug":
                print("\n🔌🔌🔌🔌 SMARTPLUG 🔌🔌🔌🔌 : " + str(data["status"]))
                # Puoi aggiungere qui eventuali azioni su Record o database
                # Esempio: Record.set_smartplug_status(data["status"])
            else:
                print("Unknown resource:", self.resource)
        except (KeyError, json.JSONDecodeError) as e:
            print("Observer error:", e)