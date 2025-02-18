#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include<WiFi.h>
#include <ArduinoJson.h>
#include <WebServer.h>

unsigned long startTime; 
unsigned long cycleTime;     // Variable to store the start time
const unsigned long ventilationInterval = 10 * 1000; 
const unsigned long pauseInterval=30*60*1000;
// const unsigned long pauseInterval = 10 * 60 * 1000; 
bool manualVentilationFlag=false;
bool ventilationFlag= false;
bool startedVentilationFlag=false;
bool pauseFlag=false;
bool counterFlag=false;
int triggeredHumidity = 80;
int counter=0;
const char* ssid = "";
const char* password = "";
const int analogPin = 34;

bool activityFlag=true;



String warningMessage="";
// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0
float aTemperature = 0.0;
float aHumidity = 0.0;


WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ESP32 Dynamic Data</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        input, button { font-size: 1.2em; margin: 5px; color:rgb(32, 50, 64); }
        #temp-value, #humid-value { font-size: 1.5em; margin: 10px; }
        h1 { color: #007BFF; }
        p { font-size: 1.5em; }
    </style>
    
</head>
<body>
    <h1>ЕСПЭХА НА ВЕНТИЛЯТОРЕ</h1>  
    <p>ТЕМПЕРАТУРА: <span id="temp-value">--</span> °C</p>
    <p>ВЛАЖНОСТЬ: <span id="humid-value">--</span> %</p>

    <h1>Set Humidity Value</h1>
    <form id="humidity-form" method="POST" action="/set-humidity">
        <input type="number" id="trigger-value" min="0" max="100" placeholder="внесите влажность включения вентиляции" required>
        <button type="submit">Установить влажность</button>
    </form>
    <button onclick="startVentilation()">Включить вентилятор</button>
    <p id="status"></p>
    <p id="warning"></p>

    
    
    <script>   

            document.getElementById("humidity-form").onsubmit = async function(event) {
            event.preventDefault();  // Prevent form from submitting normally
            const humidityValue = document.getElementById("trigger-value").value;
            const response = await fetch('/set-humidity', {
                method: 'POST',
                body: JSON.stringify({ humidity: humidityValue }),
                headers: { 'Content-Type': 'application/json' }
            });
             if (response.ok) {
                        const message = await response.text();
                        document.getElementById("status").innerText = "Влажность установлена на: " + message;
                    } else {
                        document.getElementById('status').innerText = "Ошибка.";
                    }
              };     

        async function startVentilation() {
            const response = await fetch('/start-ventilation', { method: 'POST' });
            const statusText = await response.text();
            document.getElementById('status').innerText = statusText;
            }

        async function updateTemperature() {
            const response = await fetch('/temperature');
            const data = await response.text();
            document.getElementById('temp-value').innerText = data;
        }
        async function updateHumidity() {
            const response = await fetch('/humidity');
            const data = await response.text();
            document.getElementById('humid-value').innerText = data;
        }
        async function errorWarning() {
            const response = await fetch('/warning');
            const data = await response.text();
            document.getElementById('warning').innerText = data;
        }

        setInterval(updateHumidity, 1000);
        setInterval(updateTemperature, 1000);
        setInterval(errorWarning,10000);
        
    </script>
   

</body>
</html>
)rawliteral";

void handleSetHumidity() {
     if (server.hasArg("plain")) {
        String body = server.arg("plain");

        // Parse JSON data
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("humidity")) {
            triggeredHumidity = doc["humidity"];
            delay(1000);
            Serial.println(triggeredHumidity);
            server.send(200, "text/plain", "Влажность установлена на " + String(triggeredHumidity));
        } else {
            server.send(400, "text/plain", "Invalid JSON data");
        }
    } else {
        server.send(400, "text/plain", "No data received");
    }

}

void handleManualVentilation() {
    manualVentilationFlag = true;
    server.send(200, "text/plain", "ON");
}



void serverSettings(){
   server.on("/", []() {
        server.send(200, "text/html", htmlPage);
    });
   server.on("/temperature", []() {
        server.send(200, "text/plain", String(aTemperature, 2));
    });

     server.on("/humidity", []() {
        server.send(200, "text/plain", String(aHumidity, 2));
    });
    server.on("/warning", []() {
        server.send(200, "text/plain", warningMessage);
    });
    server.on("/set-humidity", HTTP_POST, handleSetHumidity);
    server.on("/start-ventilation", HTTP_POST, handleManualVentilation);


    server.begin();
    Serial.println("Server started!");
}







SensirionI2cSht4x sensor;

static char errorMessage[64];
static int16_t error;

void initSensor(){

  Wire.begin();
  sensor.begin(Wire, SHT40_I2C_ADDR_44);

  sensor.softReset();
  delay(10);
  uint32_t serialNumber = 0;
  error = sensor.serialNumber(serialNumber);
  if (error != NO_ERROR) {
      Serial.print("Error trying to execute serialNumber(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("serialNumber: ");
  Serial.print(serialNumber);
  Serial.println();


}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  delay(1000);
}


void setup() {
    pinMode(14,OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }
    initWiFi();         
    initSensor();
    serverSettings();

    }



void loop() {
    server.handleClient();
    int analogValue = analogRead(analogPin);
    delay(200);
    error = sensor.measureLowestPrecision(aTemperature, aHumidity);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute measureLowestPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("aTemperature: ");
    Serial.print(aTemperature);
    Serial.print("\t");
    Serial.print("aHumidity: ");
    Serial.print(aHumidity);
    Serial.println();
    
    if(analogValue<=100){
      manualVentilationFlag=true;
      Serial.println("switch is on");
    }

    if(activityFlag==true){


    unsigned long currentTime = millis();
    if((aHumidity>triggeredHumidity && ventilationFlag==false)||(manualVentilationFlag==true && startedVentilationFlag==false )){
      digitalWrite(14,HIGH);
      startTime = currentTime; 
      Serial.print("turning ventilation on " + currentTime-startTime);
      ventilationFlag=true;
      manualVentilationFlag=false;
      startedVentilationFlag=true;
      if(counterFlag==false){
        counterFlag=true;
        cycleTime=currentTime;
      }
      if(currentTime-cycleTime<=pauseInterval*7 +1000*60 && counterFlag==true){
        counter++;
      }
      if(currentTime-cycleTime>pauseInterval*7 &&counterFlag==true){
        if(counter>=7){
            warningMessage="ОШИБКА:вентелятор дует больше 7 раз подряд - выключение";
            Serial.print(warningMessage);
            activityFlag=false;
            delay(100);
            digitalWrite(14,LOW);
        
        }        
        counterFlag=false;
        counter=0;
      }
    }
    if((currentTime-startTime>=ventilationInterval) && (ventilationFlag==true) && (pauseFlag==false)){
      digitalWrite(14,LOW);
      Serial.println("turning ventilation off, going on pause ");
      pauseFlag=true;
    }
    if((currentTime-startTime>=pauseInterval) && (pauseFlag==true)){
      Serial.println("pause off");
      pauseFlag=false;
      ventilationFlag=false;
      startedVentilationFlag=false;
      
    }
    }
}