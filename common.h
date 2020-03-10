void fwTicker() {
  readyForFwUpdate = true;
}

void my_delay(unsigned long ms) {
  uint32_t start = micros();

  while (ms > 0) {
    yield();
    while ( ms > 0 && (micros() - start) >= 1000) {
      ms--;
      start += 1000;
    }
  }
}

void setup_wifi() {
  int count = 0;
  my_delay(50);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MQTT_DEVICE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    my_delay(250);
    Serial.print(".");
    count++;
  }

  espClient.setTrustAnchors(&caCertX509);
  espClient.setKnownKey(&key);
  espClient.allowSelfSignedCerts();
  espClient.setFingerprint(fp);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
}

void setupCommon() {
  Serial.begin(115200);
  pinMode(WATCHDOG_PIN, OUTPUT); 

  setup_wifi();

  ticker_fw.attach_ms(FW_UPDATE_INTERVAL_SEC * 1000, fwTicker);

}

String WiFi_macAddressOf(IPAddress aIp) {
  if (aIp == WiFi.localIP())
    return WiFi.macAddress();

  if (aIp == WiFi.softAPIP())
    return WiFi.softAPmacAddress();

  return String("00-00-00-00-00-00");
}

void checkForUpdates() {

  String clientMAC = WiFi_macAddressOf(espClient.localIP());

  Serial.print("MAC: ");
  Serial.println(clientMAC);
  clientMAC.replace(":", "-");
  String filename = clientMAC.substring(9);
  String firmware_URL = String(UPDATE_SERVER) + filename + String(FIRMWARE_VERSION);
  String current_firmware_version_URL = String(UPDATE_SERVER) + filename + String("-current_version");

  HTTPClient http;

  http.begin(current_firmware_version_URL);
  int httpCode = http.GET();
  
  if ( httpCode == 200 ) {

    String newFirmwareVersion = http.getString();
    newFirmwareVersion.trim();
    
    Serial.print( "Current firmware version: " );
    Serial.println( FIRMWARE_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFirmwareVersion );
    
    if(newFirmwareVersion.substring(1).toFloat() > String(FIRMWARE_VERSION).substring(1).toFloat()) {
      Serial.println( "Preparing to update" );
      String new_firmware_URL = String(UPDATE_SERVER) + filename + newFirmwareVersion + ".bin";
      Serial.println(new_firmware_URL);
      t_httpUpdate_return ret = ESPhttpUpdate.update( new_firmware_URL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
         break;
      }
    }
    else {
      Serial.println("Already on latest firmware");  
    }
  }
  else {
    Serial.print("GET RC: ");
    Serial.println(httpCode);
  }
}

void resetWatchdog() {
  digitalWrite(WATCHDOG_PIN, HIGH);
  my_delay(20);
  digitalWrite(WATCHDOG_PIN, LOW);
}

String ip2Str(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

void updateTelemetry(String heartbeat) {

  String mac_address = WiFi_macAddressOf(espClient.localIP());
  
  String topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/attributes";
  String message = String("{\"firmware\": \"") + FIRMWARE_VERSION  +
            String("\", \"mac_address\": \"") + mac_address +
            String("\", \"heartbeat\": \"") + heartbeat +
            String("\", \"ip_address\": \"") + ip2Str(espClient.localIP()) + String("\"}");
  Serial.print("MQTT - ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message);
  client.publish(topic.c_str(), message.c_str(), true);

  topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/state";
  message = String(MQTT_DEVICE) + FIRMWARE_VERSION + "  |  " + ip2Str(espClient.localIP()) + "  |  " + String(heartbeat);
  Serial.print("MQTT - ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message);
  client.publish(topic.c_str(), message.c_str(), true);

}

void registerTelemetry() {
  String topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/config";
  String message = String("{\"name\": \"") + HA_TELEMETRY + "-" + MQTT_DEVICE +
                   String("\", \"json_attributes_topic\": \"") + String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + 
                   String("/attributes\", \"state_topic\": \"") + String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) +
                   String("/state\"}");
  Serial.print("MQTT - ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message.c_str());

  client.publish(topic.c_str(), message.c_str(), true);  
  
}

void reconnect() {
  //Reconnect to Wifi and to MQTT. If Wifi is already connected, then autoconnect doesn't do anything.
  Serial.print("Attempting MQTT connection...");
  if (client.connect(MQTT_DEVICE, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("connected");
    client.subscribe(MQTT_HEARTBEAT_SUB);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    my_delay(5000);
  }
}
