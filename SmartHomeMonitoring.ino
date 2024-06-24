/*****************************************************************************************************************************
**********************************    Author  : Ehab Magdy Abdullah                      *************************************
**********************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *************************************
**********************************    Youtube : https://www.youtube.com/@EhabMagdyy      *************************************
******************************************************************************************************************************/

/* Includes */
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

/* Your Wifi  ssid and password */
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_Password"

/* Firebase Configurations */
#define API_KEY "API_KEY"

#define USER_EMAIL "USER_EMAIL"
#define USER_PASSWORD "USER_PASSWORD"

#define DATABASE_URL "DATABASE_URL"

/* MQTT Server -> Mosquitto */
const char* mqtt_server = "test.mosquitto.org";

/* Macros for devices pin */
#define FLAME_SENSOR_PIN        34
#define DHT_SENSOR_PIN          32
#define PHOTORESISTOR_PIN       33

/* I'm using DHT22, if you use DHT11 you can change DHT22 to DHT11 */
#define DHT_SENSOR_TYPE      DHT22
DHT dht(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

uint16_t flameSensor = 0;
uint8_t humidity = 0;
uint8_t temperature = 0;
uint16_t light_intensity = 0;
char buffer[5];

WiFiClient espClient;
PubSubClient client(espClient);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

void setup_wifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void reconnect() 
{ 
  while(!client.connected()) 
  {
      Serial.println("Attempting MQTT connection...");

      if(client.connect("ESPClient")) 
      {
          Serial.println("Connected");
          client.subscribe("Ehab/Home/Flame");
          client.subscribe("Ehab/Home/Temperature");
          client.subscribe("Ehab/Home/Humidity");
          client.subscribe("Ehab/Home/Lights");
      } 
      else 
      {
          Serial.print("Failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);
      }
    }
}

void setup()
{
    Serial.begin(115200);          // Serial Monitor
    setup_wifi();                  // Wifi setup
    dht.begin();                   // DHT Sensor setup

    /* Firebase Setup */
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    Firebase.reconnectNetwork(true);
    Firebase.begin(&config, &auth);
    Firebase.setDoubleDigits(5);
    config.timeout.serverResponse = 10 * 1000;

    /* Node-RED Setup */
    client.setServer(mqtt_server, 1883);
}

void loop()
{
    /******************************************     Reading Sensors value     ***********************************************/
    flameSensor = analogRead(FLAME_SENSOR_PIN);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    light_intensity = analogRead(PHOTORESISTOR_PIN);

    flameSensor = map(flameSensor, 0, 4095, 100, 0);
    light_intensity = map(light_intensity, 0, 4095, 0, 100);

    /*****************************************    Sending Data to Firebase RTDB    ******************************************/
    /* Firebase.ready() should be called repeatedly to handle authentication tasks */
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0))
    {
        sendDataPrevMillis = millis();
        
        // Setting Temperature Reading
        if(Firebase.RTDB.setInt(&fbdo, "/Home/Temperature", temperature)){
            Serial.print("Temp: ");
            Serial.print(temperature);
        }
        else{ Serial.println("FAILED: " + fbdo.errorReason()); }
        // Setting Humidity Reading
        if(Firebase.RTDB.setInt(&fbdo, "/Home/Humidity", humidity)){
            Serial.print("C - Humidity: ");
            Serial.print(humidity);
        }
        else{ Serial.println("FAILED: " + fbdo.errorReason()); }
        // Setting Flame Reading
        if(Firebase.RTDB.setInt(&fbdo, "/Home/Flame", flameSensor)){
            Serial.print("% - Flame: ");
            Serial.print(flameSensor);
        }
        else{ Serial.println("FAILED: " + fbdo.errorReason()); }
        // Setting Photoresistor Reading
        if(Firebase.RTDB.setInt(&fbdo, "/Home/Lights", light_intensity)){
            Serial.print("% - Light intensity: ");
            Serial.print(light_intensity);
            Serial.print("%\n");
        }
        else{ Serial.println("FAILED: " + fbdo.errorReason()); }
    }

    /******************************************     Sending Data to Node-RED     *******************************************/
    if(!client.connected()) { reconnect(); }

    // Publishing Flame Sensor Reading
    String buff = String(flameSensor);
    buff += "%";
    buff.toCharArray(buffer, 5);
    client.publish("Ehab/Home/Flame", buffer, false);

    // Publishing Photoresistor Reading
    buff = String(light_intensity);
    buff += "%";
    buff.toCharArray(buffer, 5);
    client.publish("Ehab/Home/Lights", buffer, false);

    // Publishing Temperature Reading
    dtostrf(temperature, 4, 0, buffer);
    client.publish("Ehab/Home/Temperature", buffer, false);

    // Publishing Humidity Reading
    dtostrf(humidity, 4, 0, buffer);
    client.publish("Ehab/Home/Humidity", buffer, false);

    delay(2000);
}
