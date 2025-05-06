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
#define LOG_MODULE "sensor"//rivedi nome
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
#define STATE_WAITSUBACKCO2         6
#define STATE_SUBSCRIBEDCO2         7
#define STATE_WAITSUBACKPM          8
#define STATE_SUBSCRIBEDPM          9
#define STATE_WAITSTART             10
#define STATE_START                 11
#define STATE_STOP                  12
#define STATE_DISCONNECTED          13

//  STATE_SUBSCRIBEDTEMP. STATE_SUBSCRIBEDCO2, STATE_SUBSCRIBEDPM were added only for major clarity but NOT required!!
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
#define CONFIG_IP_ADDR_STR_LEN  64
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
static bool co2_out_of_bounds_min = false;
static bool co2_out_of_bounds_max = false;
static bool pm_out_of_bounds_min = false;
static bool pm_out_of_bounds_max = false;
static bool temp_out_of_bounds = false;

/*variables for sensing threshold*/
static double max_temp_threshold = 25;
static double min_co2_threshold = 2.8;
static double max_co2_threshold = 3;
static double min_pm_threshold = 2;
static double max_pm_threshold = 3;
static double delta_temp = 5;
static double delta_co2 = 0.1;
static double delta_pm = 0.5;




static double mean_co2 = 2.9;
static double stddev_co2 = 0.02;
static double mean_pm = 2.5;
static double stddev_pm = 0.10;
static double mean_temp = 23;
static double stddev_temp = 0.5 ;


/*variables for sensing present values*/
static double current_co2 = 0;
static double current_pm = 0;
static double current_temp = 0;

/* Array of topics to subscribe to */
static const char* topics[] = {"params/temperature", "params/co2", "params/pm", "pickling"}; 
#define NUM_TOPICS 3
static int current_topic_index = 0; // Index of the current topic being subscribed to
static int count_sensor_interval = 0;//counter to check the number of times the sensor is activated
static bool warning_status_active = false; //flag to check if the warning is active
static bool start = false; //flag to check if the pickling process is started. 
static bool first_publication = true; //flag to check if it is the first publication

static char payload[BUFFER_SIZE];
static char temp_str[BUFFER_SIZE]; 
static char co2_str[BUFFER_SIZE];
static char pm_str[BUFFER_SIZE];


static struct ctimer tps_sensor_timer; //timer for periodic sensing values of temperature, co2 and pm
static bool first_time = true; //flag to check if it is the first time the sensor is activated
static struct etimer reconnection_timer; //timer for reconnection to the broker
static int button_press_count = 0; //counter for button press
#define BUTTON_INTERVAL (CLOCK_SECOND * 3) //interval for button press
static struct etimer button_timer; //timer for button press

#define SENSOR_INTERVAL (CLOCK_SECOND * 10) //interval for sensing values of temperature, co2 and pm
#define MONITORING_INTERVAL (CLOCK_SECOND * 5) //interval for monitoring the values of temperature, co2 and pm if they are out of bounds

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

//idem for co2
static double generate_random_co2() {
    return generate_gaussian_noise(mean_co2, stddev_co2);
}

//idem for pm
static double generate_random_pm() {
    return generate_gaussian_noise(mean_pm, stddev_pm);
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

//to be continued...!!!!

//function that is called by the timer to check the state of the MQTT client
static void sensor_callback(void *ptr){
    if(state != STATE_START)//verify if the sensor is started
        return;
    leds_off(LEDS_BLUE);
    leds_off(LEDS_RED);
    leds_on(LEDS_GREEN);//NB: LEDS_GREEN is used to indicate that the sensor is active and the values are being sensed

    // Generate random values for temperature, co2 and pm2,5
    current_temp = generate_random_temp();
    current_co2 = generate_random_co2();
    current_pm = generate_random_pm();

//conversion of the values to string with 2 decimal digits
    sprintf(temp_str, "%d.%02d", (int)current_temp, (int)((current_temp - (int)current_temp) * 100 + 0.5));
    sprintf(co2_str, "%d.%02d", (int)current_co2, (int)((current_co2 - (int)current_co2) * 100 + 0.5));
    sprintf(pm_str, "%d.%02d", (int)current_pm, (int)((current_pm - (int)current_pm) * 100 + 0.5));

    LOG_INFO("Sensing finished: temperature: %s, co2: %s, pm: %s\n", temp_str, co2_str, pm_str);

    if(!warning_status_active){
    /*for each parameter, check if the values are out of bounds*/    
      if(current_temp > max_temp_threshold)
          temp_out_of_bounds = true;
      else
          temp_out_of_bounds = false;

      if(current_co2 < min_co2_threshold)
          co2_out_of_bounds_min = true;
      else if(current_co2 > max_co2_threshold)
          co2_out_of_bounds_max = true;
      else{
          co2_out_of_bounds_min = false;
          co2_out_of_bounds_max = false;
      }

      if(current_pm < min_pm_threshold)
          pm_out_of_bounds_min = true;
      else if(current_pm > max_pm_threshold)
          pm_out_of_bounds_max = true;
      else{
          pm_out_of_bounds_min = false;
          pm_out_of_bounds_max = false;
      }

          
    }
    
//2 possible cases:
//if the warning status is already ON, check if the values are in the range [min_value + delta ; maxvalue] or [minvalue ; max_value - delta]
    if(warning_status_active){

      if(temp_out_of_bounds && current_temp <= max_temp_threshold - delta_temp)
          temp_out_of_bounds = false;

      if(co2_out_of_bounds_min && current_co2 >= min_co2_threshold + delta_co2)
          co2_out_of_bounds_min = false;

      if(co2_out_of_bounds_max && current_co2 <= max_co2_threshold - delta_co2)
          co2_out_of_bounds_max = false;

      if(pm_out_of_bounds_min && current_pm >= min_pm_threshold + delta_pm)
          pm_out_of_bounds_min = false;

      if(pm_out_of_bounds_max && current_pm <= max_pm_threshold - delta_pm)
          pm_out_of_bounds_max = false;
    


      /*if ALL the values are in the range [min_value + delta ; maxvalue] or [minvalue ; max_value - delta] then publish data and set warning status off*/
      if (!temp_out_of_bounds && (!co2_out_of_bounds_min && !co2_out_of_bounds_max) && (!pm_out_of_bounds_min && !pm_out_of_bounds_max)) {
          sprintf(pub_topic, "%s", "sensor/temp_co2_sal"); // publish on the topic sensor/temp_co2_sal

          cJSON *root = cJSON_CreateObject();//create a cJSON object to store the values in JSON format 
          if (!root) {
              printf("Error creating cJSON object.\n");
              return;  
          }

          // Add temperature, co2, and pm values as strings to cJSON object
          cJSON_AddStringToObject(root, "temperature", temp_str);
          cJSON_AddStringToObject(root, "co2", co2_str);
          cJSON_AddStringToObject(root, "pm", pm_str);

          char *json_string = cJSON_PrintUnformatted(root);//convert the cJSON object to a JSON string
          if (!json_string) {//check if the conversion is successful
              cJSON_Delete(root);
              printf("Error converting cJSON object to JSON string.\n");
              return;  
          }

          sprintf(app_buffer, "%s", json_string);//copy the JSON string to the buffer application
          
          // Cleanup cJSON resources
          cJSON_Delete(root);
          free(json_string);

          // Publish JSON string
          mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
          LOG_INFO("Publishing values on %s topic. Publishing reason: warning status disabled\n", pub_topic);

          // Reset variables and set timer
          warning_status_active = false;
          count_sensor_interval = 0;
          ctimer_set(&tps_sensor_timer, SENSOR_INTERVAL, sensor_callback, NULL);
      }

        /*else if values are NOT in range publish only after 3 times (15 seconds in warning status)*/
      else if(count_sensor_interval == 2){//if this the third time the sensor is activated in warning status, then publish the values
              
        sprintf(pub_topic, "%s", "sensor/temp_co2_sal"); // publish on the topic sensor/temp_co2_sal

              cJSON *root = cJSON_CreateObject();
              if (!root) {
                  printf("Error creating cJSON object.\n");
                  return;  
              }

              // Add temperature, co2, and pm values as strings to cJSON object
              cJSON_AddStringToObject(root, "temperature", temp_str);
              cJSON_AddStringToObject(root, "co2", co2_str);
              cJSON_AddStringToObject(root, "pm", pm_str);

              char *json_string = cJSON_PrintUnformatted(root); 
              if (!json_string) {
                  cJSON_Delete(root);
                  printf("Error converting cJSON object to JSON string.\n");
                  return;  
              }

              
                sprintf(app_buffer, "%s", json_string);
                
              // Cleanup cJSON resources
              cJSON_Delete(root);
              free(json_string);

              // Publish JSON string
              mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
              LOG_INFO("Publishing values on %s topic. Publishing reason: too much time spent without publishing in warning status\n", pub_topic);
              count_sensor_interval = 0;
              ctimer_reset(&tps_sensor_timer);
        }
        else{//if the sensor is activated in warning status but it is not the third time, then we increment the counter and reset the timer
            count_sensor_interval++;
            ctimer_reset(&tps_sensor_timer);
        }
    }   
    
    else{/*if there are detected values out of bounds for the first time then publish data and then we set warning status ON*/
        if(temp_out_of_bounds || co2_out_of_bounds_min || co2_out_of_bounds_max || pm_out_of_bounds_min || pm_out_of_bounds_max){
              sprintf(pub_topic, "%s", "sensor/temp_co2_sal"); // publish on the topic sensor/temp_co2_sal

              cJSON *root = cJSON_CreateObject();
              if (!root) {
                  printf("Error creating cJSON object.\n");
                  return;  
              }

              // Add temperature, co2, and pm values as strings to cJSON object
              cJSON_AddStringToObject(root, "temperature", temp_str);
              cJSON_AddStringToObject(root, "co2", co2_str);
              cJSON_AddStringToObject(root, "pm", pm_str);

              char *json_string = cJSON_PrintUnformatted(root); 
              if (!json_string) {
                  cJSON_Delete(root);
                  printf("Error converting cJSON object to JSON string.\n");
                  return;  
              }

                sprintf(app_buffer, "%s", json_string);
                
              // Cleanup cJSON resources
              cJSON_Delete(root);
              free(json_string);

              // Publish JSON string
              mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
              LOG_INFO("Publishing values on %s topic. Publishing reason: detected values out of bounds\n", pub_topic);
              warning_status_active = true;
              count_sensor_interval = 0;
              ctimer_set(&tps_sensor_timer, MONITORING_INTERVAL, sensor_callback, NULL);
        }
        else{//if the values are in the range [min_value + delta ; maxvalue] or [minvalue ; max_value - delta] then publish data and set warning status off
            if(first_publication){
                sprintf(pub_topic, "%s", "sensor/temp_co2_sal"); // publish on the topic sensor/temp_co2_sal

                cJSON *root = cJSON_CreateObject();
                if (!root) {
                    printf("Error creating cJSON object.\n");
                    return;  
                }

                // Add temperature, co2, and pm values as strings to cJSON object
                cJSON_AddStringToObject(root, "temperature", temp_str);
                cJSON_AddStringToObject(root, "co2", co2_str);
                cJSON_AddStringToObject(root, "pm", pm_str);

                char *json_string = cJSON_PrintUnformatted(root); 
                if (!json_string) {
                    cJSON_Delete(root);
                    printf("Error converting cJSON object to JSON string.\n");
                    return;  
                }
              
                sprintf(app_buffer, "%s", json_string);
                
                // Cleanup cJSON resources
                cJSON_Delete(root);
                free(json_string);

                // Publish JSON string
                mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
                LOG_INFO("Publishing values on %s topic. Publishing reason: first values sensed after the start command\n", pub_topic);
                first_publication = false;
                ctimer_reset(&tps_sensor_timer);
            }
          //if it's the third time the sensor is activated in normal status, then publish the values
            else if(count_sensor_interval == 3){
                sprintf(pub_topic, "%s", "sensor/temp_co2_sal"); // publish on the topic sensor/temp_co2_sal

                cJSON *root = cJSON_CreateObject();
                if (!root) {
                    printf("Error creating cJSON object.\n");
                    return;  
                }

                // Add temperature, co2, and pm values as strings to cJSON object
                cJSON_AddStringToObject(root, "temperature", temp_str);
                cJSON_AddStringToObject(root, "co2", co2_str);
                cJSON_AddStringToObject(root, "pm", pm_str);

                char *json_string = cJSON_PrintUnformatted(root); 
                if (!json_string) {
                    cJSON_Delete(root);
                    printf("Error converting cJSON object to JSON string.\n");
                    return;  
                }
              
                sprintf(app_buffer, "%s", json_string);
                
                // Cleanup cJSON resources
                cJSON_Delete(root);
                free(json_string);

                // Publish JSON string
                mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
                LOG_INFO("Publishing values on %s topic. Publishing reason: too much time spent without publishing in normal status\n", pub_topic);
                count_sensor_interval = 0;
                ctimer_reset(&tps_sensor_timer);
            }
            else{//if the sensor is activated in normal status but it is not the third time, then we increment the counter and reset the timer
                count_sensor_interval++;
                ctimer_reset(&tps_sensor_timer);
            }
        }
    }
}