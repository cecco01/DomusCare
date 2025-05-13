extern int status;
#ifndef RES_POWER_H
#define RES_POWER_H

#include "coap-engine.h"

// Dichiarazione della risorsa
extern coap_resource_t res_power;

// Dichiarazione della funzione
void res_power_get_handler(coap_message_t *request, coap_message_t *response,
                           uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

#endif /* RES_POWER_H */