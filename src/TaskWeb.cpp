#include <WiFi.h>
#include <logger.h>
#include <SPIFFS.h>

#include "Task.h"
#include "TaskWeb.h"
#include "project_configuration.h"

WebTask::WebTask() : Task(TASK_WEB, TaskWeb), http_server(80){
}

WebTask::~WebTask() {
}

bool WebTask::setup(System &system) {
  
  system.getLogger().log(logging::LoggerLevel::LOGGER_LEVEL_INFO, getName(), "Web server started.");
  
  http_server.begin();

  return true;
}

bool WebTask::loop(System &system) {

  WiFiClient client = http_server.available();

  if(client){
    unsigned long curr_time = millis();
    unsigned long prev_time = curr_time;
    system.getLogger().log(logging::LoggerLevel::LOGGER_LEVEL_INFO, getName(), "new client with IP %s.", client.localIP().toString().c_str());
    String currentLine = "";
    String header = "";
    while (client.connected() && curr_time - prev_time <= TIMEOUT) {  // loop while the client's connected
      curr_time = millis();

      // Get the header
      while(client.available() && curr_time - prev_time <= TIMEOUT){
        curr_time = millis();
        header += client.readStringUntil('\n');
        header.trim();
        header += '\n';
      }
      Serial.print(header);

      if(header.indexOf("POST /nonexistant") == 0){
        client.println("HTTP/1.0 404 Not Found");
        client.println();
      }else if(header.indexOf("POST /enableOTA") == 0){
        if(header.indexOf("OTA_Password=password") > 0){
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client.println("<link rel=\"icon\" href=\"data:,\">");
          client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>");
          client.println("<body><h1>iGate Web Server</h1>");
          client.println("<p>OTA password is correct.</p>");
          client.println("<form><input type=\"button\" value=\"Go back\" onclick=\"history.back()\"></form>");
          client.println("</body></html>");
          client.println();
        }
      }else if(header.indexOf("GET /info") == 0){
        // Display the HTML web page
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println("Connection: close");
        client.println();
        SPIFFS.begin();
        File info_html = SPIFFS.open("/info.html");
        if(!info_html || !SPIFFS.exists("/info.html")){            

          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client.println("<link rel=\"icon\" href=\"data:,\">");
          client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>");
          client.println("<body><h1>iGate Web Server</h1>");
          client.println("<p>Error with filesystem.</p>");
          client.println("</body></html>");
          client.println();
        }else{
          String page = "";
          while(info_html.available()){
            page += info_html.readString();
          }
          page.replace("$$CALLSIGN$$", system.getUserConfig()->callsign);
          page.replace("$$IP$$", client.localIP().toString() + ":" +String(client.localPort()));
          page.replace("$$AP$$", WiFi.SSID());
          page.replace("$$RSSI$$", String(WiFi.RSSI()) + "dBm");
          page.trim();
          client.println(page);
          client.println();
        }
      }else if(header.indexOf("GET") == 0){
        client.println("HTTP/1.0 404 Not Found");
        client.println("Content-type:text/html");
        client.println("Connection: close");
        client.println();
        client.println("<!DOCTYPE html><html>");
        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client.println("<link rel=\"icon\" href=\"data:,\">");              
        client.println("<body><h1>404 Not Found</h1></body></html>");
        client.println();
      }
      break;
    }

    if(curr_time-prev_time > TIMEOUT ){
      Serial.println("Client timed out...");
    }

    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  
  return true;
}
