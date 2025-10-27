/*
 * Grave RTC Controller with MP3
 * Code developed using Gemini AI
 * Author: Mauricio Martins
 * License: MIT
 *
 * This code sets up an amplifier activation controller (Output Pin 7) and
 * an MP3 player (YX5300) based on time periods defined via RTC.
 * The configuration of periods and MP3 volume is managed through a
 * simple Web Server in Access Point (AP) mode.
 */

#include <Arduino.h> 
#include <Wire.h> 
#include "Unit_RTC.h" 
#include <M5AtomS3.h> 
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h> 

// --- MP3 PLAYER LIBRARY ---
#include <YX5300_ESP32.h>
// --------------------------

// --- PINOUT CONFIGURATIONS FOR ATOMS3 ---
const int I2C_SDA_PIN = 38; 
const int I2C_SCL_PIN = 39; 
const int OUTPUT_PIN = 7; // Pin to be activated by the alarm (Amplifier/Relay)
// ----------------------------------------

// --- MP3 PLAYER (YX5300) CONFIGURATION ---
// *Ensure RX on MP3 goes to TX (6) on ESP32, and TX on MP3 goes to RX (5) on ESP32
#define MP3_RX_PIN 5 // Connected to the MP3 module's TX
#define MP3_TX_PIN 6 // Connected to the MP3 module's RX
// Track 1 is used for the file 'grave.mp3', assuming it's the first file in the root of the SD card.
const uint8_t GRAVE_MP3_TRACK_NUM = 1; 
// ------------------------------------------

// --- ACCESS POINT CREDENTIALS (Fixed IP) ---
const char* ap_ssid = "your_ssid";
const char* ap_password = "your_pass";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
// --------------------------------------------------

// --- PERSISTENCE CONFIGURATIONS (EEPROM) ---
#define EEPROM_SIZE 512 
const int CONFIG_ADDRESS = 0;
#define MAX_PERIODS 3 // Keeping the limit of 3 periods

// Struct to hold individual alarm periods
struct Period {
  int start_h = 0;
  int start_m = 0;
  int end_h = 0;
  int end_m = 0;
};

// Struct to hold all alarm configuration data
struct AlarmData {
  int num_periods = 0; 
  Period periods[MAX_PERIODS]; 
  int volume = 15; // Volume level (0-30). Default: 15 (medium)
  int signature = 0xAABBCCDD; // Signature to validate EEPROM data
};

AlarmData alarmConfig; 
// ------------------------------------------

// Time control and RTC variables
unsigned long previousMillis = 0;
const long interval = 1000; 

Unit_RTC RTC;
rtc_time_type RTCtime;
rtc_date_type RTCdate;
WebServer server(80);

// --- MP3 OBJECTS ---
YX5300_ESP32 mp3; 

bool is_alarm_active = false; 
char str_buffer[128];

// --- STATUS LED MANAGEMENT ---
// Function to set the LED color of the AtomS3
void setLEDColor(uint32_t color) { AtomS3.dis.drawpix(color); }


// --- DATA PERSISTENCE (EEPROM) ---
void loadAlarmConfig() {
    EEPROM.get(CONFIG_ADDRESS, alarmConfig); 
    
    // Check if EEPROM data is valid
    if (alarmConfig.signature != 0xAABBCCDD) {
        Serial.println("[EEPROM] Invalid data/First run. Using defaults.");
        AlarmData default_config; 
        alarmConfig = default_config;
        saveAlarmConfig(); // Save defaults immediately
    }
    
    // Ensure the loaded volume is within limits
    alarmConfig.volume = constrain(alarmConfig.volume, 0, 30);
    
    Serial.printf("[EEPROM] %d periods and volume %d loaded.\n", alarmConfig.num_periods, alarmConfig.volume);
}

void saveAlarmConfig() {
    EEPROM.put(CONFIG_ADDRESS, alarmConfig);
    if (EEPROM.commit()) {
        Serial.println("[EEPROM] Alarm configuration saved successfully.");
    } else {
        Serial.println("[EEPROM] ERROR saving configuration.");
    }
}


// --- AP Mode Setup (NTP Removed) ---
void setupAPMode() {
    Serial.println("\n\n--- STARTING IN EXCLUSIVE ACCESS POINT (AP) MODE ---");
    
    WiFi.disconnect(true); 
    WiFi.mode(WIFI_AP);
    
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(ap_ssid, ap_password); 

    Serial.printf("SSID: %s\n", ap_ssid);
    Serial.printf("Fixed IP: %s\n", AP_IP.toString().c_str());

    Serial.println("ATTENTION: RTC time must be configured manually via Web.");
}


// --- ALARM LOGIC AND LED CONTROL (GREEN/RED) ---
void checkAlarmState() {
    int now_in_minutes = RTCtime.Hours * 60 + RTCtime.Minutes;
    bool should_be_active = false;

    // Iterate through all configured periods
    for (int i = 0; i < alarmConfig.num_periods; i++) {
        Period p = alarmConfig.periods[i];
        int start_in_minutes = p.start_h * 60 + p.start_m;
        int end_in_minutes = p.end_h * 60 + p.end_m;

        // Activation logic (including overnight periods)
        if (start_in_minutes < end_in_minutes) {
            // Period within the same day
            if (now_in_minutes >= start_in_minutes && now_in_minutes < end_in_minutes) {
                should_be_active = true;
                break; 
            }
        } else if (start_in_minutes > end_in_minutes) {
            // Overnight period (passes midnight)
            if (now_in_minutes >= start_in_minutes || now_in_minutes < end_in_minutes) {
                should_be_active = true;
                break; 
            }
        }
    } 
    
    
    if (should_be_active && !is_alarm_active) {
        // Alarm is just ACTIVATING
        digitalWrite(OUTPUT_PIN, LOW); // Activates output pin (Relay ON)
        is_alarm_active = true;
        setLEDColor(0x00FF00); // GREEN: ACTIVE
        Serial.printf("[ALARM] ACTIVATED: %02d:%02d (GREEN LED / G7 LOW)\n", RTCtime.Hours, RTCtime.Minutes);
        
        // Plays the file 'grave.mp3' (track 1) in LOOP
        Serial.println("[MP3] Playing 'grave.mp3' (Track 1) in LOOP.");
        mp3.playTrackInLoop(GRAVE_MP3_TRACK_NUM); 
        
    } else if (!should_be_active && is_alarm_active) {
        // Alarm is just DEACTIVATING
        digitalWrite(OUTPUT_PIN, HIGH); // Deactivates output pin (Relay OFF)
        is_alarm_active = false;
        setLEDColor(0xFF0000); // RED: INACTIVE
        Serial.printf("[ALARM] DEACTIVATED: %02d:%02d (RED LED / G7 HIGH)\n", RTCtime.Hours, RTCtime.Minutes);
        
        // Stops playback
        Serial.println("[MP3] Stopping playback.");
        mp3.stop();
        
    } else if (should_be_active) {
        // Keeps GREEN
        setLEDColor(0x00FF00); 
    } else {
        // Keeps RED (Inactive State)
        setLEDColor(0xFF0000); 
    }
}


// --- Web Server Functions (Handlers) ---

void handleRoot() {
    // HTML optimized (CSS and Structure) - Strings remain in Portuguese for UI consistency
    String html = "<!DOCTYPE html><html><head><title>GRAVE Controller</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body { font-family: sans-serif; background: #f0f0f0; max-width: 400px; margin: 0 auto; padding: 10px; }div { background: #fff; border-radius: 5px; padding: 20px; margin-bottom: 10px; }h1 { color: #333; } p { color: #555; }form { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }label { font-weight: bold; }input[type='number'], select { width: 90%; padding: 5px; }input[type='submit'] { grid-column: 1 / -1; padding: 10px; background: #007bff; color: white; border: none; border-radius: 5px; font-size: 1em; } h3 { grid-column: 1 / -1; margin-top: 5px; margin-bottom: 5px; border-bottom: 1px solid #ccc; padding-bottom: 5px; }</style></head><body><h1>GRAVE Controller</h1>";
    
    
    // Div: Current Status and AP Information
    html += "<div><h2>Estado Atual</h2>";
    
    RTC.getTime(&RTCtime); 
    RTC.getDate(&RTCdate);
    
    // Display current RTC time/date in Portuguese
    sprintf(str_buffer, "<p>Hora RTC: <strong>%02d:%02d:%02d</strong> (Hora Local)</strong></p>",
             RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);
    html += str_buffer;
    sprintf(str_buffer, "<p>Data RTC: <strong>%02d/%02d/%04d</strong></p>",
             RTCdate.Date, RTCdate.Month, RTCdate.Year);
    html += str_buffer;
    
    // Display Amplifier/MP3 state in Portuguese
    sprintf(str_buffer, "<p>AMP / MP3 Player: <strong>%s</strong> (Volume: %d)</p>",
             is_alarm_active ? "ON / Play" : "OFF / Stop", alarmConfig.volume);
    html += str_buffer;
    
    html += "</div>";

    // --- NEW DIV: MP3 VOLUME CONTROL ---
    html += "<div><h2>Controle de Volume do MP3</h2>";
    html += "<p>Ajuste o volume (0-30). O volume atual é: <strong>" + String(alarmConfig.volume) + "</strong>.</p>";
    html += "<form action='/setvolume' method='POST' style='grid-template-columns: 1fr;'>";
    
    html += "<label for='volume'>Nível de Volume:</label>";
    html += "<input type='range' id='volume' name='v' min='0' max='30' value='" + String(alarmConfig.volume) + "' style='width: 95%; margin-top: 5px; margin-bottom: 15px;'>";
    html += "<input type='submit' value='Salvar Volume' style='grid-column: 1 / -1; margin-top: 0;'>";
    html += "</form></div>";
    // ----------------------------------------


    // Div: Define Alarm (for multiple periods)
    html += "<div><h2>Definir Períodos de Ativação</h2>";
    
    if (alarmConfig.num_periods == 0) {
        html += "<p style='color:red;'>Nenhum período de alarme ativo.</p>";
    } else {
        html += "<p>O dispositivo será ativado durante os seguintes períodos:</p><ul>"; 
        for (int i = 0; i < alarmConfig.num_periods; i++) {
            Period p = alarmConfig.periods[i];
            sprintf(str_buffer, "<li>Período %d: <strong>%02d:%02d</strong> a <strong>%02d:%02d</strong></li>", 
                    i + 1, (int)p.start_h, (int)p.start_m, (int)p.end_h, (int)p.end_m);
            html += str_buffer;
        }
        html += "</ul>";
    

    }
    
    html += "<form action='/set' method='POST'>";
    
    // Loop to generate the 3 field sets (MAX_PERIODS = 3)
    for (int i = 0; i < MAX_PERIODS; i++) {
        Period p = (i < alarmConfig.num_periods) ? alarmConfig.periods[i] : Period();
        
        html += "<h3>Período " + String(i + 1) + "</h3>";
        
        html += "<label>Hora Início:</label><label>Minuto Início:</label>";
        sprintf(str_buffer, "<input type='number' name='start_h_%d' min='0' max='23' value='%d'>", i, (int)p.start_h);
        html += str_buffer;
        sprintf(str_buffer, "<input type='number' name='start_m_%d' min='0' max='59' value='%d'>", i, (int)p.start_m);
        html += str_buffer;

        html += "<label>Hora Fim:</label><label>Minuto Fim:</label>";
        sprintf(str_buffer, "<input type='number' name='end_h_%d' min='0' max='23' value='%d'>", i, (int)p.end_h);
        html += str_buffer;
        sprintf(str_buffer, "<input type='number' name='end_m_%d' min='0' max='59' value='%d'>", i, (int)p.end_m);
        html += str_buffer;
    }
    
    html += "<p style='grid-column: 1 / -1; font-size: 0.85em;'>* Períodos definidos como 00:00 a 00:00 serão ignorados.</p>";

    html += "<input type='submit' value='Salvar Definições'>";


    html += "</form></div>";

    // --- NEW DIV: MANUAL RTC SYNCHRONIZATION ---
    html += "<div><h2>Ajustar Hora Local</h2>";
    html += "<form action='/settime' method='POST' style='grid-template-columns: 1fr 1fr 1fr; gap: 10px;'>";
    
    html += "<h3>Hora</h3>";
    html += "<label>Hora</label><label>Minuto</label><label>Segundo</label>";
    sprintf(str_buffer, "<input type='number' name='h' min='0' max='23' value='%d'>", (int)RTCtime.Hours);
    html += str_buffer;
    sprintf(str_buffer, "<input type='number' name='m' min='0' max='59' value='%d'>", (int)RTCtime.Minutes);
    html += str_buffer;
    sprintf(str_buffer, "<input type='number' name='s' min='0' max='59' value='%d'>", (int)RTCtime.Seconds);
    html += str_buffer;

    html += "<h3>Data</h3>";
    html += "<label>Dia</label><label>Mês</label><label>Ano</label>";
    sprintf(str_buffer, "<input type='number' name='d' min='1' max='31' value='%d'>", (int)RTCdate.Date);
    html += str_buffer;
    sprintf(str_buffer, "<input type='number' name='mon' min='1' max='12' value='%d'>", (int)RTCdate.Month);
    html += str_buffer;
    sprintf(str_buffer, "<input type='number' name='y' min='2024' max='2100' value='%d'>", (int)RTCdate.Year);
    html += str_buffer;

    html += "<input type='submit' value='Definir Hora e Data' style='margin-top: 10px;'>";
    html += "</form></div>";
    // ---------------------------------------------
    
    html += "</body></html>";
    
    server.send(200, "text/html", html); 
}

void handleSet() {
    // Handler to process the alarm period form submission
    if (server.method() == HTTP_POST) {
        
        AlarmData newConfig = alarmConfig; // Copy current config, including volume
        int periods_count = 0;

        for (int i = 0; i < MAX_PERIODS; i++) {
            
            String start_h_name = "start_h_" + String(i);
            String start_m_name = "start_m_" + String(i);
            String end_h_name = "end_h_" + String(i);
            String end_m_name = "end_m_" + String(i);
            
            int start_h = server.arg(start_h_name).toInt();
            int start_m = server.arg(start_m_name).toInt();
            int end_h = server.arg(end_h_name).toInt();
            int end_m = server.arg(end_m_name).toInt();

            // Constraint checks
            start_h = constrain(start_h, 0, 23);
            start_m = constrain(start_m, 0, 59);
            end_h = constrain(end_h, 0, 23);
            end_m = constrain(end_m, 0, 59);

            // Only save if any time component is non-zero
            if (start_h != 0 || start_m != 0 || end_h != 0 || end_m != 0) {
                newConfig.periods[periods_count].start_h = start_h;
                newConfig.periods[periods_count].start_m = start_m;
                newConfig.periods[periods_count].end_h = end_h;
                newConfig.periods[periods_count].end_m = end_m;
                periods_count++;
            }
        }
        
        newConfig.num_periods = periods_count;
        alarmConfig = newConfig; 
        saveAlarmConfig();       

        Serial.printf("[Web Server] %d active periods defined.\n", alarmConfig.num_periods);
        
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Method not allowed");
    }
}

// --- HANDLER TO SET VOLUME ---
void handleSetVolume() {
    if (server.method() == HTTP_POST) {
        
        int new_volume = server.arg("v").toInt();
        new_volume = constrain(new_volume, 0, 30); 
        
        if (alarmConfig.volume != new_volume) {
            alarmConfig.volume = new_volume;
            mp3.setVolume(new_volume);
            saveAlarmConfig(); 
            Serial.printf("[Web Server] MP3 volume adjusted to: %d\n", new_volume);
        }

        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Method not allowed");
    }
}
// ----------------------------------------

void handleSetTime() {
    // Handler to set the RTC time and date manually
    if (server.method() == HTTP_POST) {
        
        // Get and validate Time
        int new_h = constrain(server.arg("h").toInt(), 0, 23);
        int new_m = constrain(server.arg("m").toInt(), 0, 59);
        int new_s = constrain(server.arg("s").toInt(), 0, 59);
        
        // Get and validate Date
        int new_d = constrain(server.arg("d").toInt(), 1, 31);
        int new_mon = constrain(server.arg("mon").toInt(), 1, 12);
        int new_y = constrain(server.arg("y").toInt(), 2024, 2100); 

        // Set Time in RTC
        RTCtime.Hours = new_h;
        RTCtime.Minutes = new_m;
        RTCtime.Seconds = new_s;
        RTC.setTime(&RTCtime);
        
        // Set Date in RTC
        RTCdate.Date = new_d;
        RTCdate.Month = new_mon;
        RTCdate.Year = new_y;
        RTC.setDate(&RTCdate);
        
        Serial.printf("[Web Server] RTC Adjusted to: %02d/%02d/%04d %02d:%02d:%02d\n",
                      new_d, new_mon, new_y, new_h, new_m, new_s);

        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Method not allowed");
    }
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not found");
}

// --- Main Functions (Setup and Loop) ---

void setup() {
    Serial.begin(115200); 
    
    AtomS3.begin(true); 
    AtomS3.dis.setBrightness(100);
    
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    Serial.println("M5Atom S3 RTC Controller starting...");
    RTC.begin(); 
    
    // MP3 Player Configuration
    mp3 = YX5300_ESP32(Serial1, MP3_RX_PIN, MP3_TX_PIN);
    mp3.enableDebugging();
    
    // Define the pin as output and the initial state as INACTIVE (HIGH)
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(OUTPUT_PIN, HIGH); 

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("FATAL ERROR: Failed to initialize EEPROM.");
        while(true); 
    }
    
    loadAlarmConfig();
    
    // NEW: Set the initial MP3 Player volume
    mp3.setVolume(alarmConfig.volume);
    Serial.printf("[MP3] Initial MP3 volume set to: %d\n", alarmConfig.volume);
    
    
    // ------------------------------------------------------------------
    // --- INITIALIZATION TEST BLOCK (10 SECONDS) ---
    // ------------------------------------------------------------------
    Serial.println("\n[TEST] STARTING 10-SECOND TEST (Amplifier/MP3)...");
    
    // 1. Activate Amplifier (Relay ON)
    digitalWrite(OUTPUT_PIN, LOW); 
    
    // 2. Play MP3 File (Track 1 - grave.mp3)
    mp3.playTrackInLoop(GRAVE_MP3_TRACK_NUM); 
    
    // 3. BLUE LED to indicate TEST MODE
    setLEDColor(0x0000FF); 
    AtomS3.dis.show(); // FORCES the LED update before blocking delay
    
    // 4. Wait for 10 seconds (This is a blocking operation)
    delay(10000); 
    
    // 5. Deactivate Amplifier (Relay OFF)
    digitalWrite(OUTPUT_PIN, HIGH); 
    
    // 6. Stop MP3
    mp3.stop(); 
    
    // 7. Clear LED (set to OFF) after the test
    setLEDColor(0x000000); 
    AtomS3.dis.show(); // FORCES the LED update before proceeding
    
    Serial.println("[TEST] 10-second test concluded. Entering Normal Operation mode.");
    // ------------------------------------------------------------------

    setupAPMode(); 
    
    // --- WEB SERVER ROUTES ---
    server.on("/", HTTP_GET, handleRoot);     
    server.on("/set", HTTP_POST, handleSet);   
    server.on("/settime", HTTP_POST, handleSetTime); 
    server.on("/setvolume", HTTP_POST, handleSetVolume); 
    server.onNotFound(handleNotFound);        
    server.begin();
    
    Serial.println("Web Server started in AP Mode.");
    Serial.printf("Access http://%s\n", AP_IP.toString().c_str());
    
    RTC.getTime(&RTCtime);
    RTC.getDate(&RTCdate);
    
    // The initial alarm state check is now handled by the loop() function
    // checkAlarmState(); 
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        AtomS3.update(); 
        
        // Get current time from RTC and check alarm state
        RTC.getTime(&RTCtime);
        RTC.getDate(&RTCdate); 
        checkAlarmState(); 
    }
    
    delay(1); 
}
