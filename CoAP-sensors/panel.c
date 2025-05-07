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

#include "nomefileh.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define SERVER_EP "coap://[fd00::1]:5683"

#define MAX_REGISTRATION_RETRY 3

static int max_registration_retry = MAX_REGISTRATION_RETRY;

void client_chunk_handler(coap_message_t *response)
{
  if (response == NULL)
  {

    LOG_ERR("Request timed out\n");
  }
  else
  {

    LOG_INFO("Registration successful\n");
    max_registration_retry = 0; // if = 0 --> registration ok!

    return;
  }

  // If I'm at this point, there was some problem in the registration phasse, so we decide to try again until max_registration_retry != 0
  max_registration_retry--;
  if (max_registration_retry == 0)
    max_registration_retry = -1;
}