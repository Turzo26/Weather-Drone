
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


#define SEALEVELPRESSURE_HPA (1013.25)

#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

const int refresh=3;//webpage refreshes every 3 seconds

int UVOUT = A0; //Output from the sensor
int REF_3V3 = 12; //3.3V power on the ESP32 board

Adafruit_BMP280 bmp; // I2C

float temperature, humidity, pressure, altitude,t,h,r,uvIntensity;

DHT dht(D4, DHT22);
double T, P;
char status;
WiFiClient client;

String apiKey = "";//Enter ThingSpeak API key here
const char *ssid =  "";//Enter wifi name here
const char *pass =  "";//Enter wifi password
const char* server = "api.thingspeak.com";

ESP8266WebServer localserver(80);

//This part is for creating a webpage and displaying the data

void sendTemp() {

  String page = "<!DOCTYPE html>\n\n";
  page +="    <meta http-equiv='refresh' content='";
  page +="    <meta name=\"transparent\" content=\"true\"";
  page += String(refresh);// how often data is read
  page +="'/>\n";  
  page +="<html>\n";
   page +="<body style=\"background-color:rgba(255,255,255,0);\">\n";
       
  page +="<center>\n"; 
  page +="<p style=\"color:gray; font-size:24px;\">Temperature: ";
  page+="<span style=\"color: blue\">";
  page+=String(t);
  page += " &#xB0;C";
  page+="</span>"; 
  page +="</p>\n";
   
   page +="<p style=\"color:gray; font-size:24px;\">Humidity: ";  
  page+="<span style=\"color: blue\">";
  page += String(h);
  page += " %";
  page +="</p>\n";
  
   page +="<p style=\"color:gray; font-size:24px;\">Pressure: ";  
  page+="<span style=\"color: blue\">";;
  page += String(pressure);
  page += " mmHg";
  page+="</span>"; 
  page +="</p>\n";
  
  if(r==0)
  page +="<p style=\"color:green; font-size:24px;\">Not Raining<br/>\n";  
else
  page +="<p style=\"color:red; font-size:24px;\">Raining<br/>\n"; 
  page +="</p>\n"; 
  
  page +="<p style=\"color:gray; font-size:24px;\">UV Intensity: ";  
  page+="<span style=\"color: blue\">";
  page += String(uvIntensity);
  page+="</span>"; 
  page +="</p>\n";
  page +="</center>\n";   
  page +="</body>\n";
  page+="<div class=\"bg\"></div>";  
  page +="</html>\n";  
 localserver.send(200,  "text/html",page);

}

void handleNotFound() {
 
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += localserver.uri();
  message += "\nMethod: ";
  message += (localserver.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += localserver.args();
  message += "\n";
  for (uint8_t i = 0; i < localserver.args(); i++) {
    message += " " + localserver.argName(i) + ": " + localserver.arg(i) + "\n";
  }
  localserver.send(404, "text/plain", message);

}



void setup() {
  Serial.begin(115200);
  delay(10);
  bmp.begin(0x76);
  Wire.begin();
  dht.begin();
  WiFi.begin(ssid, pass);
  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED)
  {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    Serial.printf("Connected, mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
 Serial.print("Gateway IP: ");
 Serial.println(WiFi.gatewayIP());


    if (MDNS.begin("Lorem_Ipsum")) {
    Serial.println("MDNS responder started");
  }

  localserver.on("/", sendTemp);

  localserver.on("/inline", []() {
    localserver.send(200, "text/plain", "this works as well");
  });

  localserver.onNotFound(handleNotFound);

  localserver.begin();
  Serial.println("HTTP server started");
}



int averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 
 
  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;
 
  return(runningValue);
}
 
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
 


void loop() {

  localserver.handleClient();
  MDNS.update();
  
  temperature = bmp.readTemperature();
  pressure = bmp.readPressure() / 100.0F;
  pressure=pressure*0.75006;
  altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);


  //DHT22 sensor
   h = dht.readHumidity();
   t = dht.readTemperature();
  float t_temp,h_temp;


//This section is for getting analog value from rain sensor
  //Rain sensor
//  int r = analogRead(A0);
//  r = map(r, 0, 1024, 0, 100);

int rain_state=digitalRead(D3);
if(rain_state==HIGH)
  r=0;
  else
  r=1;

  int uvLevel = averageAnalogRead(UVOUT);
  int refLevel = averageAnalogRead(REF_3V3);
  
  float outputVoltage = 3.3 / refLevel * uvLevel;
  
 uvIntensity = mapfloat(outputVoltage, 0.99, 2.8, 0.0, 15.0); //Convert the voltage to a UV intensity level



//This section is for uploading data to Thingspeak 
  if (client.connect(server, 80)) {
        if (!isnan(h) || !isnan(t))
            {
              String postStr = apiKey;
              postStr += "&field1=";
              postStr += String(t);
              postStr += "&field2=";
              postStr += String(h);
              
              postStr += "&field3=";
              postStr += String(pressure);
              postStr += "&field4=";
              postStr += String(r);
              postStr += "&field5=";
              postStr += String(uvIntensity);
              postStr += "\r\n\r\n\r\n\r\n\r\n";
          
              client.print("POST /update HTTP/1.1\n");
              client.print("Host: api.thingspeak.com\n");
              client.print("Connection: close\n");
              client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
              client.print("Content-Type: application/x-www-form-urlencoded\n");
              client.print("Content-Length: ");
              client.print(postStr.length());
              client.print("\n\n\n\n");
              client.print(postStr);
            }
          
              Serial.print("Temperature: ");
              Serial.println(t);
              Serial.print("Humidity: ");
              Serial.println(h);
              Serial.print("absolute pressure: ");
              Serial.print(pressure,3);
              Serial.println("mmHg");
              Serial.print("Rain: ");
              Serial.println(r);
              Serial.print("UV Intensity (mW/cm^2): ");
              Serial.println(uvIntensity);
      
        
        client.stop();
        delay(1000);
  }
}
