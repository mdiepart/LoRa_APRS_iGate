#include <WiFi.h>
#include <logger.h>

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
    static unsigned long curr_time = millis();
    static unsigned long prev_time = curr_time;
    system.getLogger().log(logging::LoggerLevel::LOGGER_LEVEL_INFO, getName(), "new client with IP %s.", client.localIP().toString().c_str());
    String currentLine = "";
    String header = "";
    while (client.connected() && curr_time - prev_time <= 2000) {  // loop while the client's connected
      curr_time = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            String page;
            if (header.indexOf("GET /page1") >= 0) {
              page="page1";
            } 
            else if (header.indexOf("GET /page2") >= 0) {
              page = "page2";
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;}");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>iGate Web Server</h1>");
            
            client.println("<p>You currently are on page "+page+".</p>");

            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
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
