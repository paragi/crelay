/******************************************************************************
 * 
 * Relay card control utility: Main module
 * 
 * Description:
 *   This software is used to controls different type of relays cards.
 *   There are 3 ways to control the relays:
 *    1. via command line
 *    2. via web interface using a browser
 *    3. via HTTP API using a client application
 * 
 * Author:
 *   Ondrej Wisniewski (ondrej.wisniewski *at* gmail.com)
 *
 * Build instructions:
 *   make
 *   sudo make install
 * 
 * Last modified:
 *   14/01/2019
 *
 * Copyright 2015-2019, Ondrej Wisniewski 
 * 
 * This file is part of crelay.
 * 
 * crelay is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with crelay.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *****************************************************************************/ 
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "data_types.h"
#include "config.h"
#include "relay_drv.h"

#define VERSION "0.14"
#define DATE "2019"

/* HTTP server defines */
#define SERVER "crelay/"VERSION
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define API_URL "gpio"
#define DEFAULT_SERVER_PORT 8000

/* HTML tag definitions */
#define RELAY_TAG "pin"
#define STATE_TAG "status"
#define SERIAL_TAG "serial"

#define CONFIG_FILE "/etc/crelay.conf"

/* Global variables */
config_t config;

static char rlabels[MAX_NUM_RELAYS][32] = {"My appliance 1", "My appliance 2", "My appliance 3", "My appliance 4",
                                           "My appliance 5", "My appliance 6", "My appliance 7", "My appliance 8"};                                       

/**********************************************************
 * Function: config_cb()
 * 
 * Description:
 *           Callback function for handling the name=value
 *           pairs returned by the conf_parse() function
 * 
 * Returns:  0 on success, <0 otherwise
 *********************************************************/
static int config_cb(void* user, const char* section, const char* name, const char* value)
{
   config_t* pconfig = (config_t*)user;
   
   #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
   if (MATCH("HTTP server", "server_iface")) 
   {
      pconfig->server_iface = strdup(value);
   } 
   else if (MATCH("HTTP server", "server_port")) 
   {
      pconfig->server_port = atoi(value);
   } 
   else if (MATCH("HTTP server", "relay1_label")) 
   {
      pconfig->relay1_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay2_label")) 
   {
      pconfig->relay2_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay3_label")) 
   {
      pconfig->relay3_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay4_label")) 
   {
      pconfig->relay4_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay5_label")) 
   {
      pconfig->relay5_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay6_label")) 
   {
      pconfig->relay6_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay7_label")) 
   {
      pconfig->relay7_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "relay8_label")) 
   {
      pconfig->relay8_label = strdup(value);
   } 
   else if (MATCH("HTTP server", "pulse_duration")) 
   {
      pconfig->pulse_duration = atoi(value);
   }
   else if (MATCH("GPIO drv", "num_relays")) 
   {
      pconfig->gpio_num_relays = atoi(value);
   } 
   else if (MATCH("GPIO drv", "active_value")) 
   {
      pconfig->gpio_active_value = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay1_gpio_pin")) 
   {
      pconfig->relay1_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay2_gpio_pin")) 
   {
      pconfig->relay2_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay3_gpio_pin")) 
   {
      pconfig->relay3_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay4_gpio_pin")) 
   {
      pconfig->relay4_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay5_gpio_pin")) 
   {
      pconfig->relay5_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay6_gpio_pin")) 
   {
      pconfig->relay6_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay7_gpio_pin")) 
   {
      pconfig->relay7_gpio_pin = atoi(value);
   } 
   else if (MATCH("GPIO drv", "relay8_gpio_pin")) 
   {
      pconfig->relay8_gpio_pin = atoi(value);
   } 
   else if (MATCH("Sainsmart drv", "num_relays")) 
   {
      pconfig->sainsmart_num_relays = atoi(value);
   } 
   else 
   {
      syslog(LOG_DAEMON | LOG_WARNING, "unknown config parameter %s/%s\n", section, name);
      return -1;  /* unknown section/name, error */
   }
   return 0;
}


/**********************************************************
 * Function: exit_handler()
 * 
 * Description:
 *           Handles the cleanup at reception of the 
 *           TERM signal.
 * 
 * Returns:  -
 *********************************************************/
static void exit_handler(int signum)
{
   syslog(LOG_DAEMON | LOG_NOTICE, "Exit crelay daemon\n");
   exit(EXIT_SUCCESS);
}

                                           
/**********************************************************
 * Function send_headers()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void send_headers(FILE *f, int status, char *title, char *extra, char *mime, 
                  int length, time_t date)
{
   time_t now;
   char timebuf[128];
   
   fprintf(f, "%s %d %s\r\n", PROTOCOL, status, title);
   fprintf(f, "Server: %s\r\n", SERVER);
   //fprintf(f, "Access-Control-Allow-Origin: *\r\n"); // TEST For test only
   now = time(NULL);
   strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
   fprintf(f, "Date: %s\r\n", timebuf);
   if (extra) fprintf(f, "%s\r\n", extra);
   if (mime) fprintf(f, "Content-Type: %s; charset=utf-8\r\n", mime);
   if (length >= 0) fprintf(f, "Content-Length: %d\r\n", length);
   if (date != -1)
   {
      strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&date));
      fprintf(f, "Last-Modified: %s\r\n", timebuf);
   }
   fprintf(f, "Connection: close\r\n");
   fprintf(f, "\r\n");
}


/**********************************************************
 * Function java_script_src()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void java_script_src(FILE *f)
{
   fprintf(f, "<script type='text/javascript'>\r\n");
   fprintf(f, "function switch_relay(checkboxElem){\r\n");
   fprintf(f, "   var status = checkboxElem.checked ? 1 : 0;\r\n");
   fprintf(f, "   var pin = checkboxElem.id;\r\n");
   fprintf(f, "   var url = '/gpio?pin='+pin+'&status='+status;\r\n");
   fprintf(f, "   var xmlHttp = new XMLHttpRequest();\r\n");
   fprintf(f, "   xmlHttp.onreadystatechange = function () {\r\n");
   fprintf(f, "      if (this.readyState < 4)\r\n");
   fprintf(f, "         document.getElementById('status').innerHTML = '';\r\n");
   fprintf(f, "      else if (this.readyState == 4) {\r\n"); 
   fprintf(f, "         if (this.status == 0) {\r\n");
   fprintf(f, "            document.getElementById('status').innerHTML = \"Network error\";\r\n");
   fprintf(f, "            checkboxElem.checked = (status==0);\r\n");
   fprintf(f, "         }\r\n");
   fprintf(f, "         else if (this.status != 200) {\r\n");
   fprintf(f, "            document.getElementById('status').innerHTML = this.statusText;\r\n");
   fprintf(f, "            checkboxElem.checked = (status==0);\r\n");
   fprintf(f, "         }\r\n");
   // TODO: add update of all relays status here (according to API response in xmlHttp.responseText) 
   fprintf(f, "      }\r\n");
   fprintf(f, "   }\r\n");
   fprintf(f, "   xmlHttp.open( 'GET', url, true );\r\n");
   fprintf(f, "   xmlHttp.send( null );\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, "</script>\r\n");
}


/**********************************************************
 * Function style_sheet()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void style_sheet(FILE *f)
{
   fprintf(f, "<style>\r\n");
   fprintf(f, ".switch {\r\n");
   fprintf(f, "  position: relative;\r\n");
   fprintf(f, "  display: inline-block;\r\n");
   fprintf(f, "  width: 60px;\r\n");
   fprintf(f, "  height: 34px;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, ".switch input {\r\n"); 
   fprintf(f, "  opacity: 0;\r\n");
   fprintf(f, "  width: 0;\r\n");
   fprintf(f, "  height: 0;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, ".slider {\r\n");
   fprintf(f, "  position: absolute;\r\n");
   fprintf(f, "  cursor: pointer;\r\n");
   fprintf(f, "  top: 0;\r\n");
   fprintf(f, "  left: 0;\r\n");
   fprintf(f, "  right: 0;\r\n");
   fprintf(f, "  bottom: 0;\r\n");
   fprintf(f, "  background-color: #ccc;\r\n");
   fprintf(f, "  -webkit-transition: .4s;\r\n");
   fprintf(f, "  transition: .4s;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, ".slider:before {\r\n");
   fprintf(f, "  position: absolute;\r\n");
   fprintf(f, "  content: \"\";\r\n");
   fprintf(f, "  height: 26px;\r\n");
   fprintf(f, "  width: 26px;\r\n");
   fprintf(f, "  left: 4px;\r\n");
   fprintf(f, "  bottom: 4px;\r\n");
   fprintf(f, "  background-color: white;\r\n");
   fprintf(f, "  -webkit-transition: .4s;\r\n");
   fprintf(f, "  transition: .4s;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, "input:checked + .slider {\r\n");
   fprintf(f, "  background-color: #2196F3;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, "input:focus + .slider {\r\n");
   fprintf(f, "  box-shadow: 0 0 1px #2196F3;\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, "input:checked + .slider:before {\r\n");
   fprintf(f, "  -webkit-transform: translateX(26px);\r\n");
   fprintf(f, "  -ms-transform: translateX(26px);\r\n");
   fprintf(f, "  transform: translateX(26px);\r\n");
   fprintf(f, "}\r\n");
   fprintf(f, "</style>\r\n");   
}


/**********************************************************
 * Function web_page_header()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void web_page_header(FILE *f)
{
   /* Send http header */
   send_headers(f, 200, "OK", NULL, "text/html", -1, -1);   
   fprintf(f, "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\r\n");
   fprintf(f, "<html><head><title>Relay Card Control</title>\r\n");
   style_sheet(f);
   java_script_src(f);
   fprintf(f, "</head>\r\n");
   
   /* Display web page heading */
   fprintf(f, "<body><table style=\"text-align: left; width: 460px; background-color: #2196F3; font-family: Helvetica,Arial,sans-serif; font-weight: bold; color: white;\" border=\"0\" cellpadding=\"2\" cellspacing=\"2\">\r\n");
   fprintf(f, "<tbody><tr><td>\r\n");
   fprintf(f, "<span style=\"vertical-align: top; font-size: 48px;\">Relay Card Control</span><br>\r\n");
   fprintf(f, "<span style=\"font-size: 16px; color: rgb(204, 255, 255);\">Remote relay card control <span style=\"font-style: italic; color: white;\">made easy</span></span>\r\n");
   fprintf(f, "</td></tr></tbody></table><br>\r\n");  
}


/**********************************************************
 * Function web_page_footer()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void web_page_footer(FILE *f)
{
   /* Display web page footer */
   fprintf(f, "<table style=\"text-align: left; width: 460px; background-color: #2196F3;\" border=\"0\" cellpadding=\"2\" cellspacing=\"2\"><tbody>\r\n");
   fprintf(f, "<tr><td style=\"vertical-align: top; text-align: center;\"><span style=\"font-family: Helvetica,Arial,sans-serif; color: white;\"><a style=\"text-decoration:none; color: white;\" href=http://ondrej1024.github.io/crelay>crelay</a> | version %s | %s</span></td></tr>\r\n",
           VERSION, DATE);
   fprintf(f, "</tbody></table></body></html>\r\n");
}   


/**********************************************************
 * Function web_page_error()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void web_page_error(FILE *f)
{    
   /* No relay card detected, display error message on web page */
   fprintf(f, "<br><table style=\"text-align: left; width: 460px; background-color: yellow; font-family: Helvetica,Arial,sans-serif; font-weight: bold; color: black;\" border=\"0\" cellpadding=\"2\" cellspacing=\"2\">\r\n");
   fprintf(f, "<tbody><tr style=\"font-size: 20px; font-weight: bold;\">\r\n");
   fprintf(f, "<td>No compatible relay card detected !<br>\r\n");
   fprintf(f, "<span style=\"font-size: 14px; color: grey;  font-weight: normal;\">This can be due to the following reasons:\r\n");
   fprintf(f, "<div>- No supported relay card is connected via USB cable</div>\r\n");
   fprintf(f, "<div>- The relay card is connected but it is broken</div>\r\n");
   fprintf(f, "<div>- There is no GPIO sysfs support available or GPIO pins not defined in %s\r\n", CONFIG_FILE);
   fprintf(f, "<div>- You are running on a multiuser OS and don't have root permissions\r\n");
   fprintf(f, "</span></td></tbody></table><br>\r\n");
}   


/**********************************************************
 * Function read_httppost_data()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
int read_httppost_data(FILE* f, char* data, size_t datalen)
{
   char buf[256];
   int data_len=0;
   
   /* POST request: data is provided after the page header.
    * Therefore we skip the header and then read the form
    * data into the buffer.
    */
   *data = 0;
   while (fgets(buf, sizeof(buf), f) != NULL)
   {
      //printf("%s", buf);
      /* Find length of form data */
      if (strstr(buf, "Content-Length:"))
      {
         data_len=atoi(buf+strlen("Content-Length:"));
         //printf("DBG: data length is %d\n", data_len);
      }
      
      /* Find end of header (empty line) */
      if (!strcmp(buf, "\r\n")) break;
   }

   /* Make sure we're not trying to overwrite the buffer */
   if (data_len >= datalen)
     return -1;

   /* Get form data string */
   if (!fgets(data, data_len+1, f)) return -1;
   *(data+data_len) = 0;
   
   return data_len;
}


/**********************************************************
 * Function read_httpget_data()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
int read_httpget_data(char* buf, char* data, size_t datalen)
{
   char *datastr;
         
   /* GET request: data (if any) is provided in the first line
    * of the page header. Therefore we first check if there is
    * any data. If so we read the data into a buffer.
    *
    * Note that we may truncate the input if it's too long.
    */
   *data = 0;
   if ((datastr=strchr(buf, '?')) != NULL)
   {
       strncpy(data, datastr+1, datalen);
   }
   
   return strlen(data);
}


/**********************************************************
 * Function process_http_request()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
int process_http_request(int sock)
{
   FILE *fin, *fout;
   char buf[256];
   char *method;
   char *url;
   char formdata[64];
   char *datastr;
   int  relay=0;
   int  i;
   int  formdatalen;
   char com_port[MAX_COM_PORT_NAME_LEN];
   char* serial=NULL;
   uint8_t last_relay=FIRST_RELAY;
   relay_state_t rstate[MAX_NUM_RELAYS];
   relay_state_t nstate=INVALID;
   
   formdata[0]=0;  

   /* Open file for input */
   fin = fdopen(sock, "r");
   
   /* Read  first line of request header which contains 
    * the request method and url seperated by a space
    */
   if (!fgets(buf, sizeof(buf), fin)) 
   {
      fclose(fin);
      return -1;
   }
   //printf("********** Raw data ***********\n");
   //printf("%s", buf);
   
   method = strtok(buf, " ");
   if (!method) 
   {
      fclose(fin);
      return -1;
   }
   //printf("method: %s\n", method);
   
   url = strtok(NULL, " ");
   if (!url)
   {
      fclose(fin);
      return -2;
   }
   //printf("url: %s\n", url);
   
   /* Check the request method we are dealing with */
   if (strcasecmp(method, "POST") == 0)
   {
      formdatalen = read_httppost_data(fin, formdata, sizeof(formdata));
   }
   else if (strcasecmp(method, "GET") == 0)
   {
      formdatalen = read_httpget_data(url, formdata, sizeof(formdata));
   }
   else
   {
      fclose(fin);
      return -3;
   }
   
   //printf("DBG: form data: %s\n", formdata);
   
   /* Open file for output */
   fout = fdopen(sock, "w");

   /* Send an error if we failed to read the form data properly */
   if (formdatalen < 0) {
     send_headers(fout, 500, "Internal Error", NULL, "text/html", -1, -1);
     fprintf(fout, "ERROR: Invalid Input. \r\n");
     goto done;
   }

   /* Get values from form data */
   if (formdatalen > 0) 
   {
      int found=0;
      //printf("DBG: Found data: ");
      datastr = strstr(formdata, RELAY_TAG);
      if (datastr) {
         relay = atoi(datastr+strlen(RELAY_TAG)+1);
         //printf("relay=%d", relay);
         found++;
      }
      datastr = strstr(formdata, STATE_TAG);
      if (datastr) {
         nstate = atoi(datastr+strlen(STATE_TAG)+1);
         //printf("%sstate=%d", found ? ", " : "", nstate);
      }
      datastr = strstr(formdata, SERIAL_TAG);
      if (datastr) {
         serial = datastr+strlen(SERIAL_TAG)+1;
         datastr = strstr(serial, "&");
         if (datastr)
            *datastr = 0;
         //printf("%sserial=%s", found ? ", " : "", serial);
      }
      //printf("\n\n");
   }
   
   /* Check if a relay card is present */
   if (crelay_detect_relay_card(com_port, &last_relay, serial, NULL) == -1)
   {
      if (strstr(url, API_URL))
      {
         /* HTTP API request, send response */
         send_headers(fout, 503, "No compatible device detected", NULL, "text/plain", -1, -1);
         fprintf(fout, "ERROR: No compatible device detected");
      }
      else
      {  
         /* Web page request */
         web_page_header(fout);
         web_page_error(fout);
         web_page_footer(fout);
      }     
   }
   else
   {  
      /* Process form data */
      if (formdatalen > 0)
      {
         if ((relay != 0) && (nstate != INVALID))
         {
            /* Perform the requested action here */
            if (nstate==PULSE)
            {
               /* Generate pulse on relay switch */
               crelay_get_relay(com_port, relay, &rstate[relay-1], serial);
               if (rstate[relay-1] == ON)
               {
                  crelay_set_relay(com_port, relay, OFF, serial);
                  sleep(config.pulse_duration);
                  crelay_set_relay(com_port, relay, ON, serial);
               }
               else
               {
                  crelay_set_relay(com_port, relay, ON, serial);
                  sleep(config.pulse_duration);
                  crelay_set_relay(com_port, relay, OFF, serial);
               }
            }
            else
            {
               /* Switch relay on/off */
               crelay_set_relay(com_port, relay, nstate, serial);
            }
         }
      }
      
      /* Read current state for all relays */
      for (i=FIRST_RELAY; i<=last_relay; i++)
      {
         crelay_get_relay(com_port, i, &rstate[i-1], serial);
      }
      
      /* Send response to client */
      if (strstr(url, API_URL))
      {
         /* HTTP API request, send response */
         send_headers(fout, 200, "OK", NULL, "text/plain", -1, -1);
         for (i=FIRST_RELAY; i<=last_relay; i++)
         {
            fprintf(fout, "Relay %d:%d<br>", i, rstate[i-1]);
         }
      }
      else
      {
         /* Web request */
         char cname[MAX_RELAY_CARD_NAME_LEN];
         crelay_get_relay_card_name(crelay_get_relay_card_type(), cname);
         
         web_page_header(fout);
         
         /* Display relay status and controls on web page */
         fprintf(fout, "<table style=\"text-align: left; width: 460px; background-color: white; font-family: Helvetica,Arial,sans-serif; font-weight: bold; font-size: 20px;\" border=\"0\" cellpadding=\"2\" cellspacing=\"3\"><tbody>\r\n");
         fprintf(fout, "<tr style=\"font-size: 14px; background-color: lightgrey\">\r\n");
         fprintf(fout, "<td style=\"width: 200px;\">%s<br><span style=\"font-style: italic; font-size: 12px; color: grey; font-weight: normal;\">on %s</span></td>\r\n", 
                 cname, com_port);
         fprintf(fout, "<td style=\"background-color: white;\"></td><td style=\"background-color: white;\"></td></tr>\r\n");
         for (i=FIRST_RELAY; i<=last_relay; i++)
         {
            fprintf(fout, "<tr style=\"vertical-align: top; background-color: rgb(230, 230, 255);\">\r\n");
            fprintf(fout, "<td style=\"width: 300px;\">Relay %d<br><span style=\"font-style: italic; font-size: 16px; color: grey;\">%s</span></td>\r\n", 
                    i, rlabels[i-1]);
            fprintf(fout, "<td style=\"text-align: center; vertical-align: middle; width: 100px; background-color: white;\"><label class=\"switch\"><input type=\"checkbox\" %s id=%d onchange=\"switch_relay(this)\"><span class=\"slider\"></span></label></td>\r\n", 
                    rstate[i-1]==ON?"checked":"",i);
         }
         fprintf(fout, "</tbody></table><br>\r\n");
         fprintf(fout, "<span id=\"status\" style=\"font-size: 16px; color: red; font-family: Helvetica,Arial,sans-serif;\"></span><br><br>\r\n");
         
         web_page_footer(fout);
      }
   }

 done:
   fclose(fout);
   fclose(fin);

   return 0;
}


/**********************************************************
 * Function print_usage()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
void print_usage()
{
   relay_type_t rtype;
   char cname[60];
   
   printf("crelay, version %s\n\n", VERSION);
   printf("This utility provides a unified way of controlling different types of relay cards.\n");
   printf("Supported relay cards:\n");
   for(rtype=NO_RELAY_TYPE+1; rtype<LAST_RELAY_TYPE; rtype++)
   {
      crelay_get_relay_card_name(rtype, cname);
      printf("  - %s\n", cname);
   }
   printf("\n");
   printf("The program can be run in interactive (command line) mode or in daemon mode with\n");
   printf("built-in web server.\n\n");
   printf("Interactive mode:\n");
   printf("    crelay -i | [-s <serial number>] <relay number> [ON|OFF]\n\n");
   printf("       -i print relay information\n\n");
   printf("       The state of any relay can be read or it can be changed to a new state.\n");
   printf("       If only the relay number is provided then the current state is returned,\n");
   printf("       otherwise the relays state is set to the new value provided as second parameter.\n");
   printf("       The USB communication port is auto detected. The first compatible device\n");
   printf("       found will be used, unless -s switch and a serial number is passed.\n\n");
   printf("Daemon mode:\n");
   printf("    crelay -d|-D [<relay1_label> [<relay2_label> [<relay3_label> [<relay4_label>]]]] \n\n");
   printf("       -d use daemon mode, run in foreground\n");
   printf("       -D use daemon mode, run in background\n\n");
   printf("       In daemon mode the built-in web server will be started and the relays\n");
   printf("       can be completely controlled via a Web browser GUI or HTTP API.\n");
   printf("       The config file %s will be used, if present.\n", CONFIG_FILE);
   printf("       Optionally a personal label for each relay can be supplied as command\n");
   printf("       line parameter which will be displayed next to the relay name on the\n");
   printf("       web page.\n\n");
   printf("       To access the web interface point your Web browser to the following address:\n");
   printf("       http://<my-ip-address>:%d\n\n", DEFAULT_SERVER_PORT);
   printf("       To use the HTTP API send a POST or GET request from the client to this URL:\n");
   printf("       http://<my-ip-address>:%d/%s\n\n", DEFAULT_SERVER_PORT, API_URL);
}


/**********************************************************
 * Function main()
 * 
 * Description:
 * 
 * Parameters:
 * 
 *********************************************************/
int main(int argc, char *argv[])
{
      relay_state_t rstate;
      char com_port[MAX_COM_PORT_NAME_LEN];
      char cname[MAX_RELAY_CARD_NAME_LEN];
      char* serial=NULL;
      uint8_t num_relays=FIRST_RELAY;
      relay_info_t *relay_info;
      int argn = 1;
      int err;
      int i = 1;

   if (argc==1)
   {
      print_usage();
      exit(EXIT_SUCCESS);
   }
   
   if (!strcmp(argv[1],"-d") || !strcmp(argv[1],"-D"))
   {
      /*****  Daemon mode *****/
      
      struct sockaddr_in sin;
      struct in_addr iface;
      int port=DEFAULT_SERVER_PORT;
      int sock;
      int i;
      
      iface.s_addr = INADDR_ANY;

      
      openlog("crelay", LOG_PID|LOG_CONS, LOG_USER);
      syslog(LOG_DAEMON | LOG_NOTICE, "Starting crelay daemon (version %s)\n", VERSION);
   
      /* Setup signal handlers */
      signal(SIGINT, exit_handler);   /* Ctrl-C */
      signal(SIGTERM, exit_handler);  /* "regular" kill */
   
      /* Load configuration from .conf file */
      memset((void*)&config, 0, sizeof(config_t));
      if (conf_parse(CONFIG_FILE, config_cb, &config) >= 0) 
      {
         syslog(LOG_DAEMON | LOG_NOTICE, "Config parameters read from %s:\n", CONFIG_FILE);
         syslog(LOG_DAEMON | LOG_NOTICE, "***************************\n");
         if (config.server_iface != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "server_iface: %s\n", config.server_iface);
         if (config.server_port != 0)     syslog(LOG_DAEMON | LOG_NOTICE, "server_port: %u\n", config.server_port);
         if (config.relay1_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay1_label: %s\n", config.relay1_label);
         if (config.relay2_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay2_label: %s\n", config.relay2_label);
         if (config.relay3_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay3_label: %s\n", config.relay3_label);
         if (config.relay4_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay4_label: %s\n", config.relay4_label);
         if (config.relay5_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay5_label: %s\n", config.relay5_label);
         if (config.relay6_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay6_label: %s\n", config.relay6_label);
         if (config.relay7_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay7_label: %s\n", config.relay7_label);
         if (config.relay8_label != NULL) syslog(LOG_DAEMON | LOG_NOTICE, "relay8_label: %s\n", config.relay8_label);
         if (config.pulse_duration != 0)  syslog(LOG_DAEMON | LOG_NOTICE, "pulse_duration: %u\n", config.pulse_duration);
         if (config.gpio_num_relays != 0) syslog(LOG_DAEMON | LOG_NOTICE, "gpio_num_relays: %u\n", config.gpio_num_relays);
         if (config.gpio_active_value >= 0) syslog(LOG_DAEMON | LOG_NOTICE, "gpio_active_value: %u\n", config.gpio_active_value);
         if (config.relay1_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay1_gpio_pin: %u\n", config.relay1_gpio_pin);
         if (config.relay2_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay2_gpio_pin: %u\n", config.relay2_gpio_pin);
         if (config.relay3_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay3_gpio_pin: %u\n", config.relay3_gpio_pin);
         if (config.relay4_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay4_gpio_pin: %u\n", config.relay4_gpio_pin);
         if (config.relay5_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay5_gpio_pin: %u\n", config.relay5_gpio_pin);
         if (config.relay6_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay6_gpio_pin: %u\n", config.relay6_gpio_pin);
         if (config.relay7_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay7_gpio_pin: %u\n", config.relay7_gpio_pin);
         if (config.relay8_gpio_pin != 0) syslog(LOG_DAEMON | LOG_NOTICE, "relay8_gpio_pin: %u\n", config.relay8_gpio_pin);
         if (config.sainsmart_num_relays != 0) syslog(LOG_DAEMON | LOG_NOTICE, "sainsmart_num_relays: %u\n", config.sainsmart_num_relays);
         syslog(LOG_DAEMON | LOG_NOTICE, "***************************\n");
         
         /* Get relay labels from config file */
         if (config.relay1_label != NULL) strcpy(rlabels[0], config.relay1_label);
         if (config.relay2_label != NULL) strcpy(rlabels[1], config.relay2_label);
         if (config.relay3_label != NULL) strcpy(rlabels[2], config.relay3_label);
         if (config.relay4_label != NULL) strcpy(rlabels[3], config.relay4_label);
         if (config.relay5_label != NULL) strcpy(rlabels[4], config.relay5_label);
         if (config.relay6_label != NULL) strcpy(rlabels[5], config.relay6_label);
         if (config.relay7_label != NULL) strcpy(rlabels[6], config.relay7_label);
         if (config.relay8_label != NULL) strcpy(rlabels[7], config.relay8_label);
         
         /* Get listen interface from config file */
         if (config.server_iface != NULL)
         {
            if (inet_aton(config.server_iface, &iface) == 0)
            {
               syslog(LOG_DAEMON | LOG_NOTICE, "Invalid iface address in config file, using default value");
            }
         }
         
         /* Get listen port from config file */
         if (config.server_port > 0)
         {
            port = config.server_port;
         }

      }
      else
      {
         syslog(LOG_DAEMON | LOG_NOTICE, "Can't load %s, using default parameters\n", CONFIG_FILE);
      }

      /* Ensure pulse duration is valid **/
      if (config.pulse_duration == 0)
      {
         config.pulse_duration = 1;
      }
      
      /* Parse command line for relay labels (overrides config file)*/
      for (i=0; i<argc-2 && i<MAX_NUM_RELAYS; i++)
      {
         strcpy(rlabels[i], argv[i+2]);
      }         
      
      /* Start build-in web server */
      sock = socket(AF_INET, SOCK_STREAM, 0);
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = iface.s_addr;
      sin.sin_port = htons(port);
      if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) != 0)
      {
         syslog(LOG_DAEMON | LOG_ERR, "Failed to bind socket to port %d : %s", port, strerror(errno));
         exit(EXIT_FAILURE);         
      }
      if (listen(sock, 5) != 0)
      {
         syslog(LOG_DAEMON | LOG_ERR, "Failed to listen to port %d : %s", port, strerror(errno));
         exit(EXIT_FAILURE);         
      }
      
      syslog(LOG_DAEMON | LOG_NOTICE, "HTTP server listening on %s:%d\n", inet_ntoa(iface), port);      

      if (!strcmp(argv[1],"-D"))
      {
         /* Daemonise program (send to background) */
         if (daemon(0, 0) == -1) 
         {
            syslog(LOG_DAEMON | LOG_ERR, "Failed to daemonize: %s", strerror(errno));
            exit(EXIT_FAILURE);
         }
         syslog(LOG_DAEMON | LOG_NOTICE, "Program is now running as system daemon");
      }

      /* Init GPIO pins in case they have been configured */
      crelay_detect_relay_card(com_port, &num_relays, NULL, NULL);
      
      while (1)
      {
         int s;
         
         /* Wait for request from web client */
         s = accept(sock, NULL, NULL);
         if (s < 0) break;
         
         /* Process request */
         process_http_request(s);
         close(s);
      }
      
      close(sock);
   }
   else
   {
      /*****  Command line mode *****/
      
      if (!strcmp(argv[argn],"-i"))
      {
         /* Detect all cards connected to the system */
         if (crelay_detect_all_relay_cards(&relay_info) == -1)
         {
            printf("No compatible device detected.\n");
            return -1;
         }
         printf("\nDetected relay cards:\n");
         while (relay_info->next != NULL)
         {
            crelay_get_relay_card_name(relay_info->relay_type, cname);
            printf("  #%d\t%s (serial %s)\n", i++ ,cname, relay_info->serial);
            relay_info = relay_info->next;
            // TODO: free relay_info list memory
         }
         
         exit(EXIT_SUCCESS);
      }
   
      if (!strcmp(argv[argn], "-s"))
      {
         if (argc > (argn+1))
         {
            serial = malloc(sizeof(char)*strlen(argv[argn+1])+1);
            strcpy(serial, argv[argn+1]);
            argn += 2;
         }
         else
         {
            print_usage();
            exit(EXIT_FAILURE);            
         }
      }

      if (crelay_detect_relay_card(com_port, &num_relays, serial, NULL) == -1)
      {
         printf("** No compatible device detected **\n");
         
         if(geteuid() != 0)
         {
            puts("You might not have permissions to use the wanted device.");
            puts("If the device is connected, check what group the device belongs to.");
            puts("You may find the device group with \"ls -al /dev/<device node name>\"");
            puts("You can add a group to a user with \"usermod -a -G <group name> <user name>\"");
            
            /** Bad very not good pracis from windows, to require root priv 
            printf("\nWarning: this program is currently not running with root privileges !\n");
            printf("Therefore it might not be able to access your relay card communication port.\n");
            printf("Consider invoking the program from the root account or use \"sudo ...\"\n");
            **/
         }
         exit(EXIT_FAILURE);
      }

      switch (argc)
      {
         case 2:
         case 4:
            /* GET current relay state */
            if (crelay_get_relay(com_port, atoi(argv[argn]), &rstate, serial) == 0)
               printf("Relay %d is %s\n", atoi(argv[argn]), (rstate==ON)?"on":"off");
            else
               exit(EXIT_FAILURE);
            break;
            
         case 3:
         case 5:
            /* SET new relay state */
            if (!strcmp(argv[argn+1],"on") || !strcmp(argv[argn+1],"ON"))
               err = crelay_set_relay(com_port, atoi(argv[argn]), ON, serial);
            else if (!strcmp(argv[argn+1],"off") || !strcmp(argv[argn+1],"OFF"))
               err = crelay_set_relay(com_port, atoi(argv[argn]), OFF, serial);
            else 
            {
               print_usage();
               exit(EXIT_FAILURE);
            }
            if (err)
               exit(EXIT_FAILURE);
            break;
            
         default:
            print_usage();
      }
   }
   
   exit(EXIT_SUCCESS);
}
