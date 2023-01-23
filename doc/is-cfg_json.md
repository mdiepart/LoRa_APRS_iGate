* callsign -> Igate's callsign. It is recommended to use the SSID « -10 ».

* network 
    * DHCP → Enables automatic IP addess attribution to the module. "True" by default.
    * static → Network parameters to use if not using DHCP.
    * hostname → Hostname to use on the network. By default it will be the callsign above, but if "overwrite" is set to "true" then "name" will be used.

* wifi 
    * active → Enables or disables Wi-Fi. "True" by default.
    * AP → List of Wi-Fi access points to which the module can connect. For each additional AP you have to add a pair of SSID and password values. Only supports 2.4 GHz APs.
        * SSID → SSID of the AP to conenct to.
        * password → Password of the AP to connect to.

* beacon
    * message → This message will be transmitted with each beacon emitted by the igate. If you choose to enable beacon TX via LoRa (see below) keep it short.
    * position → GPS coordinates of the iGate's location. Use WSG84 coordinates in decimal format (i.e. 1.23456 or -12.3456).
    * use_gps → Use the GPS module (if one is present) for the GPS coordinates. "False" by default.
    * timeout → Time (in minutes) between beacons transmissions (to APRS-IS or to RF).

* aprs_is
    * active → Enables connection to aprs-is servers. "True" by default.
    * passcode → Passcode corresponding to the callsign of the iGate. Mandatory to connect to aprs-is server. Can be generated here : https://apps.magicbug.co.uk/passcode/.
    * server → URL of the APRS server to use. More information here : http://www.aprs-is.net/APRSServers.aspx.
    * port → Port to use for the APRS server.
* digi 
    * active → Enables digipeater functionality. If this module is connected to internet, do not enable it to avoid congestion on the frequency.
    * beacon → Allows the igate to transmit beacon packets via RF. "False" by default. Be sure to set "tx_enable" to "true" if you enable this.

* lora
    * frequency_rx → Frequency to listen to (in Hz). 433775000 by default.
    * gain_rx → LNA gain in reception (valid range is 0 for automatic gain control or any integer between 1 (max gain) and 6 (min gain)). 0 by default.
    * frequency_tx → Frequency (in Hz) to transmit to. 433775000 by default.
    * power → Power (in dBm) of the emitted RF signal (valid range any integrr between 2 and 17, or the number 20 for high-power output).
    * spreading_factor → SF parameter to use for the LoRa modulation. 12 by default.
    * signal_bandwidth → Bandwidth to use for the LoRa modulation (in Hz). 125000 by default.
    * coding_rate → CR parameter to use for the LoRa modulation. 5 by default.
    * tx_enable → Enables the TX output of the iGate. If set to "true" and "digi → beacon" is set to "true", the iGate will transmit the beacons via LoRa.
* display
    * always_on → If "true", keep the OLED screen always on. If "false" the screen will turn-off after "timeout" seconds.
    * timeout → Number of seconds before turning the screen of if "always_on" is set to "false".
    * overwrite_pin → Either "0" or a pin number. The pin will be configured as input with pull-up. Pulling it down will enable the screen.
    * turn_180 → Rotates the content on the screen by 180°.

* ftp 
    * active → Enables internal FTP server to access the files in the internal flash memory (like configuration file). FTP protocol is not secured. "False" by default.
    * user → "name" and "password" pairs per user allowed to connect to FTP.

*	MQTT
    * active → Enables publication of data via MQTT protocol. "False" by default.

* syslog
    * active → Enables logs publication via syslog protocol. You should leave this on "false".
* webserver 
    * active → Enables the internal web server.
    * port → TCP port to use for the web server.
    * password → Login password for the web server.
* OTA
    * active → Enables the OTA service of the module (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
    * enableViaWeb → If set to "true", the iGate will only be listening for OTA updates once the correct password have been entered in the OTA section of the web server. It set to "false", the iGate will be always listening.
    * port → Port to use for the OTA update.
    * password → Password to enter in the web interface to enable the OTA service.
* packet_logger 
    * active → Enables the internal packet logger.
    * number_lines → Number of lines to keep in each log file.
    * files_history → Max number of older log files to keep in the flash memory. The older files will be moved to "packets.log.x" where x ranges from 0 to files_history-1 (from the latest to the oldest files).
    * tail_length → Maximum number of packets to display in the log section of the web server. Does not affect the number of lines in the downloaded file.

* ntp_server → Address of an NTP server that the module will querry to get the current time. If your network posesses a reliable NTP server you can modify this, otherwise it is highly recommended to keep the default value (pool.ntp.org).
