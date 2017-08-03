#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <EEPROM.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

char* mqtt_server = "192.168.1.22"; //Raspberry Pi
char* mqtt_port = "1883";
char* mqtt_user = "";
char* mqtt_pass = "";

WiFiClient espClient;
PubSubClient client(espClient);

const char* outTopic = "homebridge/to/set";
const char* bootTopic = "homebridge/boot";
const char* inTopic1 = "homebridge/from/set";
const char* inTopic2 = "homebridge/from/get";
char tmp [1];

int relay1_pin = D5;
int relay2_pin = D6;
int button_pin = D3;
bool relay1State = 0;
bool relay2State = 0;

// Instantiate a Bounce object :
Bounce debouncer = Bounce();

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);              // Begin eeprom to store on/off state
  pinMode(relay1_pin, OUTPUT);     // Initialize the relay pin as an output
  pinMode(relay2_pin, OUTPUT);     // Initialize the relay pin as an output
  pinMode(button_pin, INPUT);     // Initialize the button pin as an input
  relay1State = EEPROM.read(0);
  relay2State = EEPROM.read(1);
  digitalWrite(relay1_pin, relay1State);
  digitalWrite(relay2_pin, relay2State);

  debouncer.attach(button_pin);   // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);         // Input must be low for 50 ms

  //Custom WiFiManager parameters
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_mqtt_username("username", "mqtt username", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_pass, 32);

  //Wifi setup
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  WiFi.hostname("Pat's Light Switch");
  if (!wifiManager.autoConnect("LightSwitchSetup")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_username.getValue());
  strcpy(mqtt_pass, custom_mqtt_password.getValue());
  client.setServer(atoi(mqtt_server), atoi(mqtt_port));
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  extButton();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char charpayload[length];
  for (int i=0; i < length; i++)
  {
    charpayload[i] = (char)payload[i];
  }
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (!strcmp (topic, inTopic1)) //New value from homebridge
  {
    if (!strncmp (charpayload, "{\"name\":\"Pat's Room Light\",\"characteristic\":\"On\",\"value\":0}", length))
    {
      relay1State = 0;
      digitalWrite(relay1_pin, relay1State);   // Turn the relay off
      Serial.println("relay1_pin -> LOW");
      EEPROM.write(0, relay1State);    // Write state to EEPROM
      EEPROM.commit();
    }
    else if (!strncmp (charpayload, "{\"name\":\"Pat's Room Light\",\"characteristic\":\"On\",\"value\":1}", length))
    {
      relay1State = 1;
      digitalWrite(relay1_pin, relay1State);   // Turn the relay on
      Serial.println("relay1_pin -> HIGH");
      EEPROM.write(0, relay1State);    // Write state to EEPROM
      EEPROM.commit();
    }
    else if (!strncmp (charpayload, "{\"name\":\"Pat's Room Outlet\",\"characteristic\":\"On\",\"value\":0}", length))
    {
      relay2State = 0;
      digitalWrite(relay2_pin, relay2State);   // Turn the relay off
      Serial.println("relay2_pin -> LOW");
      EEPROM.write(0, relay2State);    // Write state to EEPROM
      EEPROM.commit();
    }
    else if (!strncmp (charpayload, "{\"name\":\"Pat's Room Outlet\",\"characteristic\":\"On\",\"value\":1}", length))
    {
      relay2State = 1;
      digitalWrite(relay2_pin, relay2State);   // Turn the relay on
      Serial.println("relay2_pin -> HIGH");
      EEPROM.write(0, relay2State);    // Write state to EEPROM
      EEPROM.commit();
    }
  }
  else if (!strcmp (topic, inTopic2)) //Retreiveing value from homebridge
  {
    if (!strncmp (charpayload, "{\"name\":\"Pat's Room Light\",\"characteristic\":\"On\"}", length))
    {
      Serial.println("Light request received");
      char payloadstring[75] = "{\"name\":\"Pat's Room Light\",\"characteristic\":\"On\",\"value\": ";
      strcat(payloadstring, itoa(relay1State, tmp, 10));
      strcat(payloadstring, "}");
      client.publish(outTopic, payloadstring);
    }
    else if (!strncmp (charpayload, "{\"name\":\"Pat's Room Outlet\",\"characteristic\":\"On\"}", length))
    {
      Serial.println("Outlet request received");
      char payloadstring[74] = "{\"name\":\"Pat's Room Outlet\",\"characteristic\":\"On\",\"value\": ";
      strcat(payloadstring, itoa(relay2State, tmp, 10));
      strcat(payloadstring, "}");
      client.publish(outTopic, payloadstring);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Pat's_light_switch", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(bootTopic, "Pat's light switch booted");
      // ... and resubscribe
      client.subscribe(inTopic1);
      client.subscribe(inTopic2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for (int i = 0; i < 5000; i++) {
        extButton();
        delay(1);
      }
    }
  }
}

void extButton() {
  debouncer.update();
  // Call code if Bounce fell (transition from HIGH to LOW) :
  if ( debouncer.fell() ) {
    Serial.println("Debouncer fell");
    // Toggle relay state :
    relay1State = !relay1State;
    digitalWrite(relay1_pin, relay1State);
    EEPROM.write(0, relay1State);    // Write state to EEPROM
    EEPROM.commit();
    if (relay1State == 1) {
      client.publish(outTopic, "{\"name\":\"Pat's Room Light\",\"characteristic\":\"On\",\"value\": 1}");
    }
    else if (relay1State == 0) {
      client.publish(outTopic, "{\"name\":\"Pat's Room Outlet\",\"characteristic\":\"On\",\"value\": 0}");
    }
  }
}
