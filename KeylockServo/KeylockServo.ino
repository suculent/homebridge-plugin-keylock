#include <Servo.h> 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

int ledPin = D4;
int servoPin = D2;

unsigned long epoch = 0; // will be filled with time, possibly

int LOCKED_POSITION = 90;
int UNLOCKED_POSITION = 135;
int UNLOCKED_OFFSET = 60;

Servo myservo;

// WiFi
WiFiClient espClient;

// OTA
const char* autoconf_ssid  = "ESP8266_KEYLOCK"; // SSID in AP mode
const char* autoconf_pwd   = "PASSWORD"; // fallback to default password

// MQTT

long lastReconnectAttempt = 0;

const char* mqtt_server    = "192.168.1.21";
const char* mqtt_broadcast   = "/home";
const char* mqtt_channel   = "/keylock/A";
const int mqtt_port        = 1883;

PubSubClient mclient(espClient);

String shortIdentifier() {
  String clientId = "KEYLOCK-";
  clientId += ESP.getChipId();
  return clientId;
}

void lock() {
  myservo.write(LOCKED_POSITION);
  delay(500);
  Serial.print("Locked at position: ");
  Serial.println(myservo.read());
}

// 
void unlock() {
  
  int initialPosition = myservo.read();
  //Serial.print("Initial angle: ");
  Serial.println(initialPosition);
  
  myservo.write(initialPosition - UNLOCKED_OFFSET);
  
  int servoPosition = myservo.read();
  //Serial.print("Transitional angle: ");
  Serial.println(servoPosition);   

  // Wait until door opens
  //Serial.print("Waiting...");
  delay(3000);

  //Serial.print("Closing...");
  myservo.write(initialPosition);
  delay(500);
  
  //servoPosition = myservo.read();
  //Serial.print("Final angle: ");
  //Serial.println(servoPosition);
  //Serial.print(" ");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {

    char c_payload[length];
    memcpy(c_payload, payload, length);
    c_payload[length] = '\0';

    String s_topic = String(topic);
    String s_payload = String(c_payload);

    //Serial.print("MQTT: ");
    //Serial.println(s_payload);

    if ( s_topic == mqtt_channel ) {

      if (s_payload == "UNLOCK") {          
          char mqtt_message[256];
          epoch = millis();
          sprintf(mqtt_message, "{\"identifier\":\"keylock_A\",\"status\":\"unlocked\", \"millis\":\"%lu\", \"angle\":\"%i\"}", epoch, myservo.read());
          Serial.println(mqtt_message);
          mclient.publish(mqtt_channel, mqtt_message);
          unlock();        
          char mqtt_message2[256];  
          epoch = millis();
          sprintf(mqtt_message2, "{\"identifier\":\"keylock_A\",\"status\":\"locked\", \"millis\":\"%lu\", \"angle\":\"%i\"}", epoch, myservo.read());
          Serial.println(mqtt_message2);
          mclient.publish(mqtt_channel, mqtt_message2);
          
      } else if (s_payload == "LOCK") {          
          char mqtt_message[256];          
          epoch = millis();
          sprintf(mqtt_message, "{\"identifier\":\"keylock_A\",\"status\":\"locking\", \"millis\":\"%lu\", \"angle\":\"%i\"}", epoch, myservo.read());
          Serial.println(mqtt_message);
          mclient.publish(mqtt_channel, mqtt_message); 
          lock();
          char mqtt_message2[256];
          epoch = millis();
          sprintf(mqtt_message2, "{\"identifier\":\"keylock_A\",\"status\":\"locked\", \"millis\":\"%lu\", \"angle\":\"%i\"}", epoch, myservo.read());
          mclient.publish(mqtt_channel, mqtt_message2); 
          Serial.println(mqtt_message2);         
      }

    } else if ( s_topic == mqtt_broadcast ) {
  
        if (s_payload == "IDENTIFY") {
          mclient.publish(mqtt_channel, "CONNECTED");
          char mqtt_message[256];
          sprintf(mqtt_message, "{\"identifier\":\"keylock_A\",\"status\":\"connected\", \"millis\":\"%lu\", \"angle\":\"%i\"}", epoch, myservo.read());
          Serial.println(mqtt_message);
          mclient.publish(mqtt_broadcast, mqtt_message);
        }
    }
}

boolean reconnect()
{
    String clientId = "KEYLOCK_A_";
    clientId += ESP.getChipId();
    
    Serial.print(clientId);
    Serial.println(" reconnecting...");

    if (mclient.connect(clientId.c_str(),mqtt_channel,0,false,"DISCONNECTED")) {
      
      if (mclient.subscribe(mqtt_channel)) {
        Serial.print(mqtt_channel);
        Serial.println(" subscribed.");
      } else {
        Serial.println("Not subscribed.");        
      }
      
      mclient.publish(mqtt_broadcast, "{\"keylock_A\":\"connected\"}");     
    }
    return mclient.connected();
}

//
// NTP 
//

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

unsigned int localPort = 2390;       // local port to listen for UDP packets
IPAddress timeServerIP; // will contain fetched time.nist.gov NTP server address 
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
unsigned long ntpTime = 0;
const char* ntpServerName = "time.nist.gov";

void getNTPTime() {
  Serial.println("Getting NTP time...");
  WiFi.hostByName(ntpServerName, timeServerIP); 
  Serial.println(ntpServerName);
  Serial.println(timeServerIP);
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  fetchNTPTime();
}

int counter = 10; 

void fetchNTPTime() {
  
  int result = udp.parsePacket();
  if (!result) {
    if (counter > 0) {
      Serial.println("No NTP packet yet...");
      counter--;
      delay(1000);
      fetchNTPTime();
    }    
  } else {
    Serial.print("packet received, length=");
    Serial.println(result);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
  }
} 

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  
  // Wait for response
  delay(5000);

  int result = udp.parsePacket();
  if (!result) {
    Serial.println("Waiting for NTP response...");
  } else {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
  }
}

//
//
//

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  myservo.attach(servoPin);
  Serial.println("");
  Serial.print("Initial servo state (using as locked position): ");
  Serial.println(myservo.read());
  LOCKED_POSITION = myservo.read();

  WiFiManager wifiManager;
  wifiManager.autoConnect(autoconf_ssid,autoconf_pwd);
  //setup_ota();
  mclient.setServer(mqtt_server, mqtt_port);
  mclient.setCallback(mqtt_callback);  
  
  lastReconnectAttempt = 0;
}

int once = 0;

void loop() {
if (!mclient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect()) {
        getNTPTime();
        lastReconnectAttempt = 0;
        Serial.println("Connected.");
      } else {
        Serial.println("Not reconnected!");
      }
    }
  } else {
    mclient.loop(); // required for callback to work
  }
  delay(10);
}
