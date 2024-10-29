#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include <ThingerESP32.h>

#define USERNAME ""
#define DEVICE_ID ""
#define DEVICE_CREDENTIAL ""

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

#define MQ135_PIN 35
#define MQ5_PIN 34
#define NEOPIXEL_PIN 14
#define BUZZER_PIN 12
#define NUMPIXELS 2
#define RL 10.0 // Valor de la resistencia de carga en kOhms
#define VCC 3.3 // Voltaje de la fuente
#define RO 1.81 // Resistencia en aire limpio (calculada)

// Credenciales Wi-Fi
const char* ssid = "";
const char* password = "";

// Credenciales Twilio
const char* twilioAccountSID = "";
const char* twilioAuthToken = "";
const char* fromNumber = "whatsapp:14155238886";

// Lista de números de teléfono
const char* toNumbers[] = {
  "whatsapp:"
};
const int numNumbers = sizeof(toNumbers) / sizeof(toNumbers[0]);

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool alertSent = false;

void setup() {
  Serial.begin(115200);
  pinMode(MQ135_PIN, INPUT);
  pinMode(MQ5_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pixels.begin(); 
  pixels.show(); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");

  thing.add_wifi(ssid, password); 

  thing["sensor_data"] >> [](pson & out){
    float voltage_mq135 = readSensor(MQ135_PIN);
    float ppm_mq135 = getPPM(voltage_mq135) * 100; 
    out["voltage_mq135"] = voltage_mq135;
    out["ppm_mq135"] = ppm_mq135;

    float voltage_mq5 = readSensor(MQ5_PIN);
    float ppm_mq5 = getPPM(voltage_mq5) * 100; 
    out["voltage_mq5"] = voltage_mq5;
    out["ppm_mq5"] = ppm_mq5;
  };
}

float readSensor(int pin) {
  int sensorValue = analogRead(pin);
  float voltage = sensorValue / 4096.0 * 3.3; 
  return voltage;
}

float getPPM(float voltage) {
  if (voltage == 0) { return 0.1; } 
  voltage = abs(voltage); 
  float rs = (VCC - voltage) * RL / voltage; 
  float ratio = rs / RO;
  float ppm = pow(10, ((log10(ratio) - log10(1)) / -0.32)); 

  Serial.print("Voltaje: ");
  Serial.println(voltage);
  Serial.print("PPM Calculado: ");
  Serial.println(ppm);

  return ppm;
}

void sendWhatsAppMessage() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String message = "¡Alerta de fuga de gas detectada!";
    String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(twilioAccountSID) + "/Messages.json";
    String auth = "Basic " + base64::encode(String(twilioAccountSID) + ":" + String(twilioAuthToken));

    for (int i = 0; i < numNumbers; i++) {
      String toNumber = toNumbers[i];
      http.begin(url);
      http.addHeader("Authorization", auth);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String payload = "To=" + toNumber + "&From=" + String(fromNumber) + "&Body=" + message;

      int httpResponseCode = http.POST(payload);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error en el envío del mensaje: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }
}

void activateAlert() {
  Serial.println("¡Alerta fuga de gas detectada!");
  for (int j = 0; j < 5; j++) {
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 0, 0)); 
    }
    pixels.show();
    digitalWrite(BUZZER_PIN, HIGH); 
    delay(100);
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0)); 
    }
    pixels.show();
    digitalWrite(BUZZER_PIN, LOW); 
    delay(100);
  }
  sendWhatsAppMessage();
}

void loop() {
  thing.handle(); 

  float voltage_mq135 = readSensor(MQ135_PIN);
  float ppm_mq135 = getPPM(voltage_mq135) * 100; 
  float voltage_mq5 = readSensor(MQ5_PIN);
  float ppm_mq5 = getPPM(voltage_mq5) * 100; 

  if ((voltage_mq135 < 1.75 || voltage_mq5 < 2.25) && !alertSent) {
    activateAlert();
    alertSent = true; 
  } else if (voltage_mq135 >= 1.75 && voltage_mq5 >= 2.25) {
    Serial.println("No hay fuga");
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 255, 0)); 
    }
    pixels.show();
    digitalWrite(BUZZER_PIN, LOW); 
    alertSent = false; 
  }

  delay(1000); 
}
