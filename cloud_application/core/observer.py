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

    def __init__(self,source_address, resource):
        self.address = source_address
        self.resource = resource
        self.database = Database()
        self.start_observing()
       
    def start_observing(self):
        self.client = HelperClient(self.address)
        self.client.observe(self.resource, self.observer)
    
    def observer(self, response):#da rivedere, pensando allo scopo che vogliamo dare alla nsotra applicazione != ML

        if not response.payload:
            print("Empty response payload")
            return
        
        data = json.loads(response.payload)
        
        try:
            if self.resource == "boh": #da rivedere i nomi delle risorse
                print("\n🔄🔄🔄🔄 BOH 🔄🔄🔄🔄 : " + str(data["value"]))
                label = ""

                if label != "":
                    Record.push_failure(label)
            else:
                print("Unknown resource:", self.resource)
        except KeyError as e:
            print("KeyError:", e)