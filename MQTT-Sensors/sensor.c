/*
 * Copyright (c) 2020, Carlo Vallati, University of Pisa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "os/dev/leds.h"
#include "os/sys/log.h"
#include "mqtt-client.h"
#include "os/dev/button-hal.h"
#include "../cJSON-master/cJSON.h"

#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <locale.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "sensor-temp_humidity"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif

/*---------------------------------------------------------------------------*/
/* MQTT broker address. */
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"

static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

// Default config values
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)
#define RECONNECT_DELAY_MS (CLOCK_SECOND * 5) 


// We assume that the broker does not require authentication

/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;

#define STATE_INIT    		        0
#define STATE_NET_OK    	        1
#define STATE_CONNECTING            2
#define STATE_CONNECTED             3
#define STATE_WAITSUBACKTEMP        4
#define STATE_SUBSCRIBEDTEMP        5
#define STATE_WAITSUBACKHUM          6
#define STATE_SUBSCRIBEDHUM          7
#define STATE_WAITSUBACKSAL         8
#define STATE_SUBSCRIBEDSAL         9
#define STATE_WAITSTART             10
#define STATE_START                 11
#define STATE_STOP                  12
#define STATE_DISCONNECTED          13

//  STATE_SUBSCRIBEDTEMP. STATE_SUBSCRIBEDHUM, STATE_SUBSCRIBEDSAL were added only for major clarity but not required
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topics.
 * Make sure they are large enough to hold the entire respective string
 */
#define BUFFER_SIZE 64

static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];


// Periodic timer to check the state of the MQTT client
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
static struct etimer periodic_timer;

/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;

static struct mqtt_connection conn;

mqtt_status_t status;
char broker_address[CONFIG_IP_ADDR_STR_LEN];

/*---------------------------------------------------------------------------*/

/*Utility variables*/

/*variables to report values outside the limits */
static bool humidity_out_of_bounds_min = false;
static bool humidity_out_of_bounds_max = false;
static bool salinity_out_of_bounds_min = false;
static bool salinity_out_of_bounds_max = false;
static bool temp_out_of_bounds = false;

/*variables for sensing threshold*/
static double min_humidity_threshold = 2.8;
static double max_humidity_threshold = 3;
static double min_salinity_threshold = 2;
static double max_salinity_threshold = 3;
static double delta_temp = 5;
static double delta_humidity = 0.1;
static double delta_salinity = 0.5;
static double max_temp_threshold = 25;



static double mean_humidity = 2.9;
static double stddev_humidity = 0.02;
static double mean_salinity = 2.5;
static double stddev_salinity = 0.10;
static double mean_temp = 23;
static double stddev_temp = 0.5 ;


/*variables for sensing present values*/
static double current_humidity = 0;
static double current_salinity = 0;
static double current_temp = 0;

/* Array of topics to subscribe to */
static const char* topics[] = {"params/temperature", "params/humidity", "params/salinity", "pickling"}; 
#define NUM_TOPICS 3
static int current_topic_index = 0; // Index of the current topic being subscribed to
static int count_sensor_interval = 0;//counter to check the number of times the sensor is activated
static bool warning_status_active = false; //flag to check if the warning is active
static bool start = false; //flag to check if the pickling process is started. 
static bool first_publication = true; //flag to check if it is the first publication

static char payload[BUFFER_SIZE];
static char temp_str[BUFFER_SIZE]; 
static char humidity_str[BUFFER_SIZE];
static char salinity_str[BUFFER_SIZE];


static struct ctimer tps_sensor_timer; //timer for periodic sensing values of temperature, humidity and salinity
static bool first_time = true; //flag to check if it is the first time the sensor is activated
static struct etimer reconnection_timer; //timer for reconnection to the broker
static int button_press_count = 0; //counter for button press
#define BUTTON_INTERVAL (CLOCK_SECOND * 3) //interval for button press
static struct etimer button_timer; //timer for button press

#define SENSOR_INTERVAL (CLOCK_SECOND * 10) //interval for sensing values of temperature, humidity and salinity
#define MONITORING_INTERVAL (CLOCK_SECOND * 5) //interval for monitoring the values of temperature, humidity and salinity if they are out of bounds

/*---------------------------------------------------------------------------*/
/*Utility functions for generate random sensing values*/

// The function generates a random number from a Gaussian distribution with the given mean and standard deviation.
static double generate_gaussian_noise(double mean, double stddev) {
    static bool has_spare = false;
    static double spare;
    if (has_spare) {
        has_spare = false;
        return mean + stddev * spare;
    }

    has_spare = true;
    static double u, v, s;
    do {
        u = (rand() / ((double) RAND_MAX)) * 2.0 - 1.0;
        v = (rand() / ((double) RAND_MAX)) * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);

    s = sqrt(-2.0 * log(s) / s);
    spare = v * s;
    return mean + stddev * u * s;
}

// The function generates a random temperature value based on a Gaussian distribution with the given mean and standard deviation.
static double generate_random_temp() {
    return generate_gaussian_noise(mean_temp, stddev_temp);
    
}

//idem for humidity
static double generate_random_humidity() {
    return generate_gaussian_noise(mean_humidity, stddev_humidity);
}

//idem for salinity
static double generate_random_salinity() {
    return generate_gaussian_noise(mean_salinity, stddev_salinity);
}


// The function replaces the comma with a dot in the string. This is useful for converting the string to a double value.
void replace_comma_with_dot(char *str) {
    char *ptr = str;
    while (*ptr != '\0') {
        if (*ptr == ',') {
            *ptr = '.';
        }
        ptr++;
    }
}

//to be continued...