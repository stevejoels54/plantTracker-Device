#include <SoftwareSerial.h>
#include "RTClib.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // Data wire is plugged into pin 4 on the Arduino
#define SOIL_MOISTURE_PIN A2 // Pin where the soil moisture sensor is connected
#define LDR_PIN A1 // Pin where the LDR is connected

SoftwareSerial gsmSerial(3, 2);  // Define SoftwareSerial object for GSM module, with RX and TX pins connected to Arduino pins 10 and 11 respectively
RTC_DS3231 rtc; // Create an instance of the RTC_DS3231 class
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.

unsigned long previousMillis = 0; // Variable to store the previous time millis()
unsigned long interval = 900000; // 15 minutes in milliseconds

void setup() {
  Serial.begin(9600);          // Initialize Serial communication for debugging
  gsmSerial.begin(9600);      // Initialize GSM module Serial communication
  delay(1000);                // Delay to allow GSM module to initialize
  Serial.println("GSM Module initialized...");

  // Initialize RTC
  if (! rtc.begin()) {
      Serial.println("Couldn't find RTC");
      Serial.flush();
      while (1) delay(10);
    }
  if (rtc.lostPower()) {
      Serial.println("RTC lost power, let's set the time!");
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }

  // Start up the library for onewire Temp sensor
  sensors.begin();
    
}

void loop() {
  // Check if it's time to send data
  DateTime now = rtc.now(); // Get the current date and time from RTC
  unsigned long currentMillis = now.unixtime() * 1000; // Convert to milliseconds

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Update previousMillis

    // Read data from sensors or prepare data to be sent
    
    int temperature = readTemperature();
    Serial.print("Temperature: ");
    Serial.println(temperature);
    delay(1000); // Delay for 1 second

    int moisturePercentage = readSoilMoisturePercentage(); // Read soil moisture percentage
    Serial.print("Soil Moisture Percentage: ");
    Serial.print(moisturePercentage);
    Serial.println("%");
    delay(1000); // Delay for readability

    int lightIntensityPercentage = readLightIntensityPercentage(); // Read light intensity percentage
    Serial.print("Light Intensity Percentage: ");
    Serial.print(lightIntensityPercentage);
    Serial.println("%");
    delay(1000); // Delay for readability

    DateTime now = rtc.now(); // Get current time and date from RTC
    String readingTime = now.timestamp(DateTime::TIMESTAMP_FULL); // Get the timestamp in the desired format
    Serial.print("Reading Time: ");
    Serial.println(readingTime); // Print the reading time in the desired format
    delay(1000); // Delay for readability

    // Send data to the server

    gsmStart();
    post_data_to_server(1, temperature, lightIntensityPercentage, moisturePercentage, readingTime);

    // Print a message to indicate data has been sent
    Serial.println("Data sent to server");
  }
}

// Function to read from gsm via serial and print response
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    gsmSerial.write(Serial.read()); // Forward what Serial received to gsmSerial
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read()); // Forward what gsmSerial received to Serial
  }
}

// Function to send AT command to start HTTPS connection 
void gsmStart() {
  gsmSerial.println("AT+CGDCONT=1,\"IP\",\"internet\""); // Replace <APN> with your Access Point Name (APN) for your mobile network provider
  delay(1000);
  updateSerial();
  
  gsmSerial.println("AT+CSQ"); // Check signal strength for debugging
  delay(1000);
  updateSerial();
  
  gsmSerial.println("AT+CGATT?"); // Check GPRS attachment status
  delay(1000);
  updateSerial();
  
  gsmSerial.println("AT+CIPSHUT"); // Deactivate GPRS PDP context
  delay(1000);
  updateSerial();
  
  gsmSerial.println("AT+CIPMUX=0"); // Set single connection mode
  delay(1000);
  updateSerial();
  
  gsmSerial.println("AT+CIPSTART=\"TCP\",\"yourserver.com\",80"); // Start SSL/TLS connection to the server with URL and port
  delay(3000);
  updateSerial();
}


void post_data_to_server(int device_id, int temperature, int light, int moisture, String reading_time) {
  String url = "/add_reading/" + String(device_id) + "/" + String(temperature) + "/" + String(light) + "/" + String(moisture) + "?reading_time=" + reading_time;
  String host = "yourserver.com"; // Replace this with your actual server hostname

  int contentLength = 0;

  // Construct the HTTP POST request
  gsmSerial.println("AT+CIPSTART=\"TCP\",\"" + host + "\",80");
  delay(1000);
  gsmSerial.println("AT+CIPSEND"); // Start data sending mode
  delay(1000);
  gsmSerial.print("POST " + url + " HTTP/1.1\r\n");
  updateSerial();
  gsmSerial.print("Host: " + host + "\r\n");
  updateSerial();
  gsmSerial.print("Content-Type: application/x-www-form-urlencoded\r\n");
  updateSerial();
  gsmSerial.print("Content-Length: " + String(contentLength) + "\r\n");
  updateSerial();
  gsmSerial.print("\r\n");
  gsmSerial.print("\r\n");
  gsmSerial.write(26); // Send Ctrl+Z to indicate end of data
  delay(3000);
  updateSerial();

  // Read and print the server's response
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }
  Serial.println();
  Serial.println("Request completed...");

  // Close the connection
  gsmSerial.println("AT+CIPCLOSE");                 // Close the TCP/SSL connection
  delay(1000);
  gsmSerial.println("AT+CIPSHUT");                  // Deactivate GPRS PDP context
}

// Read temperature function
int readTemperature() {
  // Request temperatures from all devices on the bus
  sensors.requestTemperatures();
  
  // Get the temperature in Celsius from the first sensor (index 0)
  float tempC = sensors.getTempCByIndex(0);

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C) {
    // Convert float temperature to integer
    int tempInt = int(tempC);
    return tempInt;
  } else {
    // Return 0 in case of error
    return 0;
  }
}

// Read moisture function
int readSoilMoisturePercentage() {
  int soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN); // Read the raw analog value from the soil moisture sensor
  int soilMoisturePercentage = map(soilMoistureRaw, 0, 1023, 100, 0); // Map the raw value to a percentage value (100-0)
  return soilMoisturePercentage;
}

// Read light intensity function
int readLightIntensityPercentage() {
  int ldrValue = analogRead(LDR_PIN); // Read the raw analog value from the LDR
  int luxValue = map(ldrValue, 0, 1023, 0, 1000); // Map the raw value to a lux value (0-1000) based on calibration
  int percentageValue = map(luxValue, 0, 1000, 100, 0); // Map the lux value to a percentage value (0-100)
  return percentageValue;
}
