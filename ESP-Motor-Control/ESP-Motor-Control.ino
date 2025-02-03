#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

const char* ssid = "ESP_Hotspot";  // Set the ESP8266 as an access point
const char* password = "12345678";  // Hotspot password

ESP8266WebServer server(80);  // Web server running on port 80
DNSServer dnsServer;  // DNS server to handle redirection

// Pin for the motor (adjust accordingly)
const int motorPin = 5;

// Handle root endpoint and serve the HTML page
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Water Pump Remote</title>
    
    <style>               
        :root { --toggle-move: 120px; }
        * { padding: 0; margin: 0; }
        body, html { width: 100%; height: 100%; }
        body { display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 16px; background-color: rgb(255, 255, 255); }
        #status { width: 47%; text-align: center; font-size: 16px; font-weight: bold; color: rgb(24, 24, 24); background-color: #eaeaea; border: 2px solid #000; border-radius: 8px; padding: 4px; }
        #ip { width: 58%; height: 24px; background-color: rgb(24, 24, 24); border: 2px solid #ff2b4a; border-radius: 4px; color: #40ff4e; font-weight: 600; text-align: center; outline: none; padding: 0 6px; }
        #remote { width: 50%; height: 55%; border-radius: 20px; background-color: rgb(18, 18, 18); box-shadow: 0 0 8px 2px rgba(35, 35, 35, 1); display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 16px; }
        #toggle { position: relative; width: 60%; height: 70%; border-radius: 100px; border: 3px solid rgba(19, 132, 242, 0.8); transform: rotate(180deg); }
        #checkbox { display: none; }
        #toggle-btn { position: absolute; width: 100%; height: 100%; border-radius: 100px; background-color: rgb(20, 20, 20); }
        #toggle-btn::after { content: "Off"; transform: rotate(180deg); display: flex; justify-content: center; align-items: center; font-weight: 800; font-size: 22px; color: rgb(180, 180, 180); position: absolute; top: 4px; left: 4px; width: calc(100% - 8px); aspect-ratio: 1; border-radius: 100%; background: rgb(28, 28, 28); box-shadow: 0 0 8px 2px rgb(100, 100, 100); transition: transform 0.3s; }
        #checkbox:checked + #toggle-btn::after { transform: translateY(var(--toggle-move)) rotate(180deg); content: "On"; color: #15ff4d; box-shadow: 0 0 8px 2px #2bff2e; }
    </style>
</head>
<body>    
    <div id="status">Waiting for status...</div>
    
    <div id="remote">
        <input id="ip" type="text" readonly placeholder="Fetching IP...">
        
        <div id="toggle">
            <input id="checkbox" type="checkbox">
            <label for="checkbox" id="toggle-btn"></label>
        </div>        
    </div>
    
    <script>        
        const checkbox = document.getElementById("checkbox");       
        const toggleBtn = document.getElementById("toggle-btn");
        const ipinput = document.getElementById('ip');
        const statusBox = document.getElementById('status');
        let espIP = ""; // IP fetched from the server

        window.addEventListener("load", () => {
            adjustToggleSize();
            fetchESPIP();  
        });

        checkbox.addEventListener("change", () => {
            toggleMotor();
        });

        function adjustToggleSize() {
            const toggleBtnCss = window.getComputedStyle(toggleBtn, "::after");
            const toggleBtnHeight = parseInt(toggleBtnCss.getPropertyValue('height'));
            const toggleMove = checkbox.parentElement.clientHeight - toggleBtnHeight - 8;
            document.documentElement.style.setProperty('--toggle-move', toggleMove + 'px');
        }

        function fetchESPIP() {
            fetch("/get-ip")
                .then(response => response.text())
                .then(ip => {
                    if(ip.length > 20) return;
                    espIP = ip;
                    ipinput.value = espIP;                    
                    checkMotorStatus();
                })
                .catch(error => {
                    console.error('Error fetching IP:', error);
                    ipinput.value = "Error: No IP";
                });
        }

        function toggleMotor() {
            const motorStatus = checkbox.checked ? 'turnon' : 'turnoff';
            fetch(`/motor/${motorStatus}`)
                .then(response => response.text())
                .then(res => {
                    statusBox.textContent = res.length < 100 ? res : `Error: Failed to ${motorStatus}`;
                })
                .catch(error => {
                    console.error('Network error:', error);
                    statusBox.textContent = "Error: Couldn't reach server.";
                });
        }    

        function checkMotorStatus() {
            fetch(`/motor/status`)
                .then(response => response.text())
                .then(status => {
                    statusBox.textContent = status;
                    checkbox.checked = (status === "Motor is ON");
                })
                .catch(error => {
                    console.log('Error fetching motor status:', error);
                    statusBox.textContent = "Error: Could not fetch motor status.";
                });
        }
    </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Handle IP fetching endpoint
void handleGetIP() {
  String ip = WiFi.softAPIP().toString();
  server.send(200, "text/plain", ip);
}

// Handle motor turning on
void handleMotorOn() {
  digitalWrite(motorPin, HIGH);
  server.send(200, "text/plain", "Motor is ON");
}

// Handle motor turning off
void handleMotorOff() {
  digitalWrite(motorPin, LOW);
  server.send(200, "text/plain", "Motor is OFF");
}

// Handle motor status
void handleMotorStatus() {
  String status = digitalRead(motorPin) == HIGH ? "Motor is ON" : "Motor is OFF";
  server.send(200, "text/plain", status);
}

// Handle invalid routes and redirect to root
void handleNotFound() {
  String url = server.uri();
  Serial.println("404 Not Found: " + url);
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(9600);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);  // Motor is initially off

  // Set up the ESP8266 as an access point (Hotspot)
  WiFi.softAP(ssid, password);
  Serial.println("Hotspot created");

  // Set up DNS server to redirect to the ESP IP
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Serve the HTML page at root endpoint
  server.on("/", HTTP_GET, handleRoot);  // Root endpoint serving the HTML page
  server.on("/get-ip", HTTP_GET, handleGetIP);  // Return ESP IP
  server.on("/motor/turnon", HTTP_GET, handleMotorOn);  // Turn motor on
  server.on("/motor/turnoff", HTTP_GET, handleMotorOff);  // Turn motor off
  server.on("/motor/status", HTTP_GET, handleMotorStatus);  // Get motor status

  server.onNotFound(handleNotFound);  // Handle redirects if the user doesn't go to `/`

  server.begin();
  Serial.println("Server started");

  // Print ESP IP to Serial Monitor
  Serial.print("ESP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  dnsServer.processNextRequest();  // Process DNS requests
  server.handleClient();  // Handle web server requests
}