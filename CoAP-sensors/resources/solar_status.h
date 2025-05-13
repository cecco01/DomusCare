#ifndef SOLAR_STATUS_H
#define SOLAR_STATUS_H

#include "coap-engine.h"

// Dichiarazione dello stato
extern int status;

// Dichiarazione della risorsa
extern coap_resource_t res_solar_status;

// Dichiarazione della funzione handler
void res_solar_status_get_handler(coap_message_t *request, coap_message_t *response,
                                  uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

#endif /* SOLAR_STATUS_H */
