#include <WiFi.h>
#include <logger.h>
#include <SPIFFS.h>

#include "TaskOTA.h"
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

  _stateInfo = "Online";
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

      if(header.indexOf("POST /enableOTA") == 0){
        if(header.indexOf("OTA_Password=") > 0){
          //Get password from header
          int index = header.indexOf("OTA_Password=") + String("OTA_Password=").length();
          int end_line = header.indexOf("\n", index);
          String password = header.substring(index, end_line);
          password.trim();

          client.println(STATUS_200);

          //Load page and replace placeholder
          String page = loadPage("/enableOTA.html");

          if(system.getUserConfig()->web.otaPassword.equals(password)){
            std::list<Task*> tasks = system.getTaskManager().getTasks();
            for(Task *it : system.getTaskManager().getTasks()){
              if(it->getTaskId() == TaskOta){
                ((OTATask *)it)->enableOTA(5*60*1000); //Enabling OTA for 5 minutes
                system.getLogger().log(logging::LoggerLevel::LOGGER_LEVEL_INFO, getName(), "User enabled OTA for 5 minutes via web interface");
                page.replace("$$STATUS$$", "<p>OTA password is correct.</p><p>OTA Enabled for 5 minutes.</p>");

                break;
              }
            }
            page.replace("$$STATUS$$", "<p>OTA password is correct, however there was an error with the OTA task.</p>");
          }else{
            page.replace("$$STATUS$$", "<p>OTA password is invalid.</p>");
          }

          client.println(page);
        }else{
          client.stop();
        }
      }else if(header.indexOf("GET / ") == 0){
        client.println("HTTP/1.1 301 Moved Permanently");
        client.println("Location: /info");
        client.println();
      }else if(header.indexOf("GET /info") == 0){
        // Display the HTML web page
        client.println(STATUS_200);
        String page = loadPage("/info.html");
        page.replace("$$CALLSIGN$$", system.getUserConfig()->callsign);
        page.replace("$$IP$$", client.localIP().toString() + ":" +String(client.localPort()));
        page.replace("$$AP$$", WiFi.SSID());
        page.replace("$$RSSI$$", String(WiFi.RSSI()) + "dBm");

        //Build $$TASKLIST$$ string
        String tasklist = "";
        for(Task *task : system.getTaskManager().getTasks()){
          switch(task->getTaskId()){
            case TaskOta:
              switch(((OTATask *)task)->getOTAStatus()){
                case OTATask::Status::OTA_ForceDisabled:
                  tasklist += "<p class=\"task\">" + task->getName() + ": Disabled.</p>";
                  break;
                case OTATask::Status::OTA_ForceEnabled:
                  tasklist += "<p class=\"task ok\">" + task->getName() + ": Enabled.</p>";
                  break;
                case OTATask::Status::OTA_Disabled:
                  tasklist += "<p class=\"task warning\">" + task->getName() + ": Disabled. Use web interface to enable.</p>";
                  break;
                case OTATask::Status::OTA_Enabled:
                  tasklist += "<p class=\"task ok\">" + task->getName() + ": Enabled for ";
                  unsigned int seconds = ((OTATask *)task)->getTimeRemaining()/1000;
                  tasklist += String(seconds) + " more seconds.</p>";
                  break;
              }
              break;
            case TaskWifi:
                tasklist += "<p class=\"task ok\">" + 
                            task->getName() + ": connected to AP \"" +
                            WiFi.SSID() + "\". RSSI is " + String(WiFi.RSSI()) + "dBm "
                            "and IP is " + WiFi.localIP().toString().c_str() + ".</p>";
              break;
            default:
              switch(task->getState()){
                case TaskDisplayState::Okay:
                  tasklist += "<p class=\"task ok\">";
                  break;
                case TaskDisplayState::Warning:
                  tasklist += "<p class=\"task warning\">";
                  break;
                case TaskDisplayState::Error:
                  tasklist += "<p class=\"task error\">";
                  break;
              }
            tasklist += task->getName() + ": " + task->getStateInfo() + "</p>";
          }
        }

        page.replace("$$TASKLIST$$", tasklist);

        page.trim();
        client.println(page);
        client.println();
      }else{
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


    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
  }
  
  return true;
}

String WebTask::loadPage(String file){
  SPIFFS.begin();
  String pageString;
  File pageFile = SPIFFS.open(file);
  if(!pageFile || !SPIFFS.exists(file)){ 
    pageString = String("<!DOCTYPE html><html>"
                        "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                        "<link rel=\"icon\" href=\"data:,\">"
                        "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head>"
                        "<body><h1>iGate Web Server</h1>"
                        "<p>Error with filesystem.</p>"
                        "</body></html>");
  }else{
    pageString.reserve(pageFile.size());
    while(pageFile.available()){
      pageString += pageFile.readString();
    }
  }

  return pageString;
}
