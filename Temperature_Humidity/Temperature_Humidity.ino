/*
 * -----------------------------------------------------------
 * Project: Temperature and Humidity Monitor with NTP and Charts
 *
 * Description: This program reads temperature and humidity data
 *              from the AM2302 sensor and serves it as a webpage
 *              over the local network. It also logs the data
 *              and displays it with charts for analysis.
 * Author: George Papapetros
 * Date: 16/09/2024
 * Version: 1.0
 * 
 * Hardware: 
 *   - Arduino MKR WiFi 1010
 *   - AM2302 Temperature and Humidity Sensor
 *   - 10KΩ Resistor
 * 
 * Libraries Used:
 *   - WiFiNINA.h (for WiFi connection)
 *   - DHT.h (for reading data from AM2302 sensor)
 *   - NTPClient.h (for time synchronization via NTP)
 *   - WiFiUdp.h (for handling UDP requests, used with NTPClient)
 *   - ArduinoJson.h (for handling JSON data in web responses)
 * 
 * Features:
 *   - Connects to a WiFi network
 *   - Retrieves and displays temperature and humidity data every 10 minutes
 *   - Serves a local webpage with live charts for temperature and humidity
 *   - Option to manually retrieve current temperature and humidity
 *   - Real-time NTP-based time tracking
 * 
 * Usage:
 *   - Upload the program to the Arduino MKR WiFi 1010.
 *   - Connect to the specified WiFi network.
 *   - Access the local IP of the Arduino to view the data and charts.
 *
 * Do do:
 *   - Automatically adjusts for daylight saving time (DST)
 *   - Automatically set Static IP (without changing router settings)
 * -----------------------------------------------------------
 */

#include <WiFiNINA.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// DHT setup
#define DHTPIN 2           // Ο pin του AM2302
#define DHTTYPE DHT22      // Αισθητήρας DHT22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// Wi-Fi settings
char ssid[] = "YOUR_SSID";    // Το όνομα του WiFi δικτύου σου   
char pass[] = "YOUR_PASSWORD"; // Ο κωδικός του WiFi δικτύου σου
int status = WL_IDLE_STATUS;

// NTP setup
WiFiUDP ntpUDP;
// Ρύθμιση χειμερινής και θερινής ώρας
const int winterOffset = 7200;  // 2 ώρες σε δευτερόλεπτα για GMT+2 (Ελλάδα κατά τη διάρκεια της χειμερινής ώρας)
const int summerOffset = 10800; // 3 ώρες σε δευτερόλεπτα για GMT+3 (Ελλάδα κατά τη διάρκεια της θερινής ώρας)
int timeOffset = summerOffset; //<-- ΧΕΙΡΟΚΙΝΗΤΗ ΑΛΛΑΓΗ
NTPClient timeClient(ntpUDP, "gr.pool.ntp.org", timeOffset, 60000); // NTP server, offset, update interval

// Web server setup
WiFiServer server(80);

// Variables
float temperature = 0.0;
float humidity = 0.0;
unsigned long previousMillis = 0;
const long interval = 600000;  // 10 λεπτά = 600.000 ms

// Arrays to store temperature and humidity history
const int historySize = 100;
float tempHistory[historySize];
float humidityHistory[historySize];
String timeHistory[historySize];
int historyIndex = 0;

void setup() {
  // Serial communication
  Serial.begin(9600);
  while (!Serial);

  // Start DHT sensor
  dht.begin();

  // Connect to WiFi
  connectToWiFi();

  // Initialize NTP client
  timeClient.begin();
  timeClient.update();

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  printWiFiStatus();
}

void loop() {
  unsigned long currentMillis = millis();

  // Έλεγχος για ανάγνωση από τον αισθητήρα κάθε 15 λεπτά
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readSensorAndStore();
    printToSerial();
  }

  // Έλεγχος αν κάποιος client συνδέθηκε στο server
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
}

// Συνάρτηση για σύνδεση στο Wi-Fi
void connectToWiFi() {
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
}

// Συνάρτηση για ανάγνωση των δεδομένων από τον αισθητήρα
void readSensorAndStore() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  // Update NTP client
  timeClient.update();
  String currentTime = timeClient.getFormattedTime();

  if (historyIndex < historySize) {
    tempHistory[historyIndex] = temperature;
    humidityHistory[historyIndex] = humidity;
    timeHistory[historyIndex] = currentTime;
    historyIndex++;
  }
  else {
    // Shift all elements to the left by 1 to make room for the new data point
    for(int i = 0; i < historySize-1; i++){
      tempHistory[i] = tempHistory[i+1];
      humidityHistory[i] = humidityHistory[i+1];
      timeHistory[i] = timeHistory[i+1];
  }
    // Add new data to the last position
    tempHistory[historySize-1] = temperature;
    humidityHistory[historySize-1] = humidity;
    timeHistory[historySize-1] = currentTime;
  }
}

// Συνάρτηση για εκτύπωση των δεδομένων στο Serial Terminal
void printToSerial() {
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity);
  Serial.print(" %, Time: ");
  Serial.println(timeClient.getFormattedTime());
}

// Συνάρτηση για επιστροφή της τρέχουσας ώρας
String getTime() {
  return timeClient.getFormattedTime();
}

// Συνάρτηση για διαχείριση των αιτημάτων από τον client
void handleClient(WiFiClient client) {
  // Περιμένουμε να στείλει αίτημα ο client
  while (client.connected()) {
    if (client.available()) {
      String request = client.readStringUntil('\r');
      client.flush();

      if (request.indexOf("/readNow") != -1) {
        // Αν ζητηθεί ανάγνωση τώρα
        readSensorAndStore();
        printToSerial();
      }

      if (request.indexOf("/reset") != -1) {
        // Αν ζητηθεί reset
        resetData();
      }

      // HTML Response
      sendHTMLPage(client);
      break;
    }
  }

  // Κλείνουμε τη σύνδεση με τον client
  client.stop();
}

// Συνάρτηση για επαναφορά των δεδομένων
void resetData() {
  historyIndex = 0; // Διαγράφει όλα τα αποθηκευμένα δεδομένα
  Serial.println("Data reset");
}

// Συνάρτηση για την αποστολή του HTML περιεχομένου
void sendHTMLPage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  // HTML page
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  
  // Add the Chart.js script
  client.println("<head><title>Temperature and Humidity</title>");
  client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>"); // Chart.js CDN
  client.println("</head>");
  
  client.println("<body>");
  // Dark mode
  // client.println("<body style='background-color: #121212; color: white; font-weight: bold;'>");

  // Current temperature and humidity
  client.println("<h1 style='color: red;'>Temperature: " + String(temperature) + " C</h1>");
  client.println("<h1 style='color: #00BFFF;'>Humidity: " + String(humidity) + " %</h1>");
  client.println("<h2>Last Updated: " + getTime() + "</h2>");
  
  // Button for manual read
  client.println("<form action=\"/readNow\" method=\"get\">");
  client.println("<button type=\"submit\" style=\"width: 150px; height: 50px; font-size: 25px;\">Read Now</button>");
  client.println("</form>");
  client.println("<br>");

  // Button for resetting data
  client.println("<form action=\"/reset\" method=\"get\">");
  client.println("<button type=\"submit\" style=\"width: 150px; height: 50px; font-size: 25px;\">Reset Data</button>");
  client.println("</form>");

  // Graphs for temperature and humidity
  client.println("<h2>Temperature-Time Graph</h2>");
  client.println("<canvas id=\"tempGraph\" width=\"400\" height=\"200\"></canvas>");
  client.println("<h2>Humidity-Time Graph</h2>");
  client.println("<canvas id=\"humidityGraph\" width=\"400\" height=\"200\"></canvas>");

  // JavaScript for graphs
  client.println("<script>");
  client.println("var tempData = " + arrayToJson(tempHistory, historyIndex) + ";");
  client.println("var humidityData = " + arrayToJson(humidityHistory, historyIndex) + ";");
  client.println("var timeData = " + arrayToJson(timeHistory, historyIndex) + ";");

  client.println("function drawGraph(canvasId, data, label) {");
  client.println("  var ctx = document.getElementById(canvasId).getContext('2d');");
  client.println("  new Chart(ctx, {");
  client.println("    type: 'line',");
  client.println("    data: {");
  client.println("      labels: timeData,");
  client.println("      datasets: [{");
  client.println("        label: label,");
  client.println("        data: data,");
  client.println("        borderColor: 'rgba(75, 192, 192, 1)',");
  client.println("        fill: false");
  client.println("      }]");
  client.println("    },");
  client.println("    options: {");
  client.println("      scales: {");
  client.println("        x: { type: 'category' },");
  client.println("        y: { beginAtZero: true }");
  client.println("      }");
  client.println("    }");
  client.println("  });");
  client.println("}");

  client.println("drawGraph('tempGraph', tempData, 'Temperature');");
  client.println("drawGraph('humidityGraph', humidityData, 'Humidity');");
  client.println("</script>");
  
  client.println("</body></html>");
}

// Συνάρτηση για μετατροπή των δεδομένων σε JSON format
String arrayToJson(float arr[], int size) {
  String json = "[";
  for (int i = 0; i < size; i++) {
    json += String(arr[i]);
    if (i < size - 1) json += ",";
  }
  json += "]";
  return json;
}

String arrayToJson(String arr[], int size) {
  String json = "[";
  for (int i = 0; i < size; i++) {
    json += "\"" + arr[i] + "\"";
    if (i < size - 1) json += ",";
  }
  json += "]";
  return json;
}

// Συνάρτηση για εκτύπωση της διεύθυνσης IP
void printWiFiStatus() {
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}