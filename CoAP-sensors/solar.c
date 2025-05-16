#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"
#include "os/dev/leds.h"
#include "coap-blocking-api.h"

#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#else
#include "dev/button-sensor.h"
#endif

#define LOG_LEVEL LOG_LEVEL_DBG
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define SERVER_EP "coap://[fd00::1]:5683"  // Indirizzo del server

#define MAX_REGISTRATION_RETRY 3

static int max_registration_retry = MAX_REGISTRATION_RETRY;
extern void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_solar,
  "title=\"Produzione Energeticq\";rt=\"Text\"",
  res_get_handler,
  NULL,
  NULL,
  NULL);

void client_chunk_handler(coap_message_t *response){//
  if (response == NULL){
    LOG_ERR("Request timed out\n");
  }
  else{
    const uint8_t *payload = NULL;
    int len = coap_get_payload(response, &payload);
    if (len > 0){
      printf("Response received: %.*s\n", len, (const char *)payload);
    }
    else{
      LOG_INFO("Registration successful\n");
    max_registration_retry = 0; // if = 0 --> registration ok
    }
    
    return;
  }

  // If I'm at this point, there was some problem in the registration phase, so we decide to try again until max_registration_retry != 0
  max_registration_retry--;
  if (max_registration_retry == 0)
    max_registration_retry = -1;
}

/*
 * Resources to be activated need to be imported through the extern keyword.
 * The build system automatically compiles the resources in the corresponding sub-directory.
 */
 
 static struct etimer e_timer, sleep_timer;
 
 PROCESS(SolarPw_server, "SolarPw Sensor CoAP Server");
 AUTOSTART_PROCESSES(&SolarPw_server);
 
 int status = 1;
 
 PROCESS_THREAD(SolarPw_server, ev, data){
   static coap_endpoint_t main_server_ep;
   static coap_message_t request[1];
 
   PROCESS_BEGIN();
 
 #if PLATFORM_HAS_BUTTON
 #if !PLATFORM_SUPPORTS_BUTTON_HAL
   SENSORS_ACTIVATE(button_sensor);
 #endif
   printf("Press a button to switch the SolarPw status\n");
 #endif
 
   LOG_INFO("Starting SolarPw Server\n");
   coap_activate_resource(&res_solar, "SolarPw");
 
   while (max_registration_retry != 0){
     /* -------------- REGISTRATION --------------*/
     // Populate the coap_endpoint_t data structure
     coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &main_server_ep);
     // Prepare the message
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
     coap_set_header_uri_path(request, "register/");//ricontrolla se register va bene...
     // During registration, send the IP address of the solar sensor
     const char msg[] = "{\"t\": \"solar\"}";
     // Set payload
     coap_set_payload(request, (uint8_t *)msg, strlen(msg));
 
     leds_single_on(LEDS_YELLOW);
 
     COAP_BLOCKING_REQUEST(&main_server_ep, request, client_chunk_handler);
 
     /* -------------- END REGISTRATION --------------*/
     if (max_registration_retry == -1){ //if something goes wrong more than "MAX_REGISTRATION_RETRY" times then the node goes to sleep then try again
       etimer_set(&sleep_timer, 30 * CLOCK_SECOND);
       PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sleep_timer));
       max_registration_retry = MAX_REGISTRATION_RETRY;
     }
   }
 
   LOG_INFO("REGISTRATION SUCCESS\n");
   leds_single_off(LEDS_YELLOW);
 
   etimer_set(&e_timer, CLOCK_SECOND * 10);
 
   while (1){
 
     PROCESS_WAIT_EVENT();
 
     if (ev == PROCESS_EVENT_TIMER && data == &e_timer){
       if (status == 1){
         res_solar.trigger();//serve un coap client registrato alla risorsa
         LOG_INFO("res_solar event triggered\n");
       }
       etimer_reset(&e_timer);
 
 #if PLATFORM_HAS_BUTTON
 #if PLATFORM_SUPPORTS_BUTTON_HAL
     }
     else if (ev == button_hal_release_event){
 #else
     }
     else if (ev == sensors_event && data == &button_sensor){
 #endif
 
       LOG_INFO("Button pressed: switch the SolarPw status from %d to %d\n", status, !status);
 
       status = !status;
 
       if (status == 1){
         // set a timer to send the SolarPw value every 10 seconds
         etimer_set(&e_timer, CLOCK_SECOND * 10);
       }
 
 #endif /* PLATFORM_HAS_BUTTON */
     }
   }
 
   PROCESS_END();
 }



/* Dichiarazione della risorsa per il consumo energetico */


/* Implementazione del gestore GET per la risorsa */
