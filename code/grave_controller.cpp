/*
 * CÓDIGO FINAL - CONTROLADOR GRAVE RTC COM MP3
 * * Este código configura um controlador de ativação de amplificador (Output Pin 7) e
 * um MP3 player (YX5300) baseado em períodos de tempo definidos via RTC.
 * A configuração de períodos e o volume do MP3 são geridos através de um
 * Servidor Web em modo Access Point (AP).
 */

#include <Arduino.h> 
#include <Wire.h> 
#include "Unit_RTC.h" 
#include <M5AtomS3.h> 
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h> 

// --- NOVO: BIBLIOTECA DO MP3 PLAYER ---
#include <YX5300_ESP32.h>
// ------------------------------------

// --- CONFIGURAÇÕES DE PINAGEM PARA O ATOMS3 ---
const int I2C_SDA_PIN = 38; 
const int I2C_SCL_PIN = 39; 
const int OUTPUT_PIN = 7; // Pino a ser ativado pelo alarme (Amplificador)
// ------------------------------------------

// --- CONFIGURAÇÕES DO MP3 PLAYER (YX5300) ---
// *Certifique-se de que RX no MP3 vá para TX (6) no ESP32, e TX no MP3 vá para RX (5) no ESP32
#define MP3_RX_PIN 5 // Conectado ao TX do módulo MP3
#define MP3_TX_PIN 6 // Conectado ao RX do módulo MP3
// A faixa 1 é usada para o arquivo 'grave.mp3', assumindo que é o primeiro arquivo na raiz do SD.
const uint8_t GRAVE_MP3_TRACK_NUM = 1; 
// ------------------------------------------

// --- CREDENCIAIS DO ACCESS POINT (IP Fixo) ---
const char* ap_ssid = "your_ssid";
const char* ap_password = "your_password";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
// --------------------------------------------------

// --- CONFIGURAÇÕES DE PERSISTÊNCIA (EEPROM) ---
#define EEPROM_SIZE 512 
const int CONFIG_ADDRESS = 0;
#define MAX_PERIODS 3 // Mantendo o limite de 3 períodos conforme seu último código

struct Period {
  int start_h = 0;
  int start_m = 0;
  int end_h = 0;
  int end_m = 0;
};

struct AlarmData {
  int num_periods = 0; 
  Period periods[MAX_PERIODS]; 
  int volume = 15; // Nível de volume (0-30). Padrão: 15 (meio)
  int signature = 0xAABBCCDD;
};

AlarmData alarmConfig; 
// ------------------------------------------

// Variáveis de controlo de tempo e RTC
unsigned long previousMillis = 0;
const long interval = 1000; 

Unit_RTC RTC;
rtc_time_type RTCtime;
rtc_date_type RTCdate;
WebServer server(80);

// --- OBJETOS MP3 ---
YX5300_ESP32 mp3; 

bool is_alarm_active = false; 
char str_buffer[128];

// --- GESTÃO DO LED DE STATUS ---
void setLEDColor(uint32_t color) { AtomS3.dis.drawpix(color); }


// --- PERSISTÊNCIA DE DADOS (EEPROM) ---
void loadAlarmConfig() {
    EEPROM.get(CONFIG_ADDRESS, alarmConfig); 
    
    if (alarmConfig.signature != 0xAABBCCDD) {
        Serial.println("[EEPROM] Dados inválidos/Primeira execucao. Usando defaults.");
        AlarmData default_config; 
        alarmConfig = default_config;
        saveAlarmConfig(); 
    }
    
    // Garantir que o volume carregado está dentro dos limites
    alarmConfig.volume = constrain(alarmConfig.volume, 0, 30);
    
    Serial.printf("[EEPROM] %d períodos e volume %d carregados.\n", alarmConfig.num_periods, alarmConfig.volume);
}

void saveAlarmConfig() {
    EEPROM.put(CONFIG_ADDRESS, alarmConfig);
    if (EEPROM.commit()) {
        Serial.println("[EEPROM] Configuração do alarme salva com sucesso.");
    } else {
        Serial.println("[EEPROM] ERRO ao salvar configuração.");
    }
}


// --- Configuração do Modo AP (NTP Removido) ---
void setupAPMode() {
    Serial.println("\n\n--- INICIANDO EM MODO ACCESS POINT (AP) EXCLUSIVO ---");
    
    WiFi.disconnect(true); 
    WiFi.mode(WIFI_AP);
    
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(ap_ssid, ap_password); 

    Serial.printf("SSID: %s\n", ap_ssid);
    Serial.printf("IP Fixo: %s\n", AP_IP.toString().c_str());

    Serial.println("ATENÇÃO: A hora do RTC deve ser configurada manualmente via Web.");
}


// --- LÓGICA DO ALARME E CONTROLE DE LED (VERDE/VERMELHO) ---
void checkAlarmState() {
    int now_in_minutes = RTCtime.Hours * 60 + RTCtime.Minutes;
    bool should_be_active = false;

    // Itera por todos os períodos configurados
    for (int i = 0; i < alarmConfig.num_periods; i++) {
        Period p = alarmConfig.periods[i];
        int start_in_minutes = p.start_h * 60 + p.start_m;
        int end_in_minutes = p.end_h * 60 + p.end_m;

        // Lógica de ativação (incluindo períodos noturnos/overnight)
        if (start_in_minutes < end_in_minutes) {
            // Período no mesmo dia
            if (now_in_minutes >= start_in_minutes && now_in_minutes < end_in_minutes) {
                should_be_active = true;
                break; 
            }
        } else if (start_in_minutes > end_in_minutes) {
            // Período noturno (passa pela meia-noite)
            if (now_in_minutes >= start_in_minutes || now_in_minutes < end_in_minutes) {
                should_be_active = true;
                break; 
            }
        }
    } 
    
    
    if (should_be_active && !is_alarm_active) {
        // Alarme acaba de ATIVAR
        digitalWrite(OUTPUT_PIN, LOW); // Ativa pino de saída
        is_alarm_active = true;
        setLEDColor(0x00FF00); // VERDE: ATIVO
        Serial.printf("[ALARME] ATIVADO: %02d:%02d (LED VERDE / G7 LOW)\n", RTCtime.Hours, RTCtime.Minutes);
        
        // Faz playback do ficheiro 'grave.mp3' (faixa 1) EM LOOP
        Serial.println("[MP3] Tocar 'grave.mp3' (Faixa 1) em LOOP.");
        mp3.playTrackInLoop(GRAVE_MP3_TRACK_NUM); 
        
    } else if (!should_be_active && is_alarm_active) {
        // Alarme acaba de DESATIVAR
        digitalWrite(OUTPUT_PIN, HIGH); // Desativa pino de saída
        is_alarm_active = false;
        setLEDColor(0xFF0000); // VERMELHO: INATIVO
        Serial.printf("[ALARME] DESATIVADO: %02d:%02d (LED VERMELHO / G7 HIGH)\n", RTCtime.Hours, RTCtime.Minutes);
        
        // Para o playback
        Serial.println("[MP3] Parar playback.");
        mp3.stop();
        
    } else if (should_be_active) {
        // Mantém VERDE
        setLEDColor(0x00FF00); 
    } else {
        // Mantém VERMELHO (Estado Inativo)
        setLEDColor(0xFF0000); 
    }
}


// --- Funções do Servidor Web (Handlers) ---

void handleRoot() {
    // HTML otimizado (CSS e Estrutura)
    String html = "<!DOCTYPE html><html><head><title>GRAVE Controller</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body { font-family: sans-serif; background: #f0f0f0; max-width: 400px; margin: 0 auto; padding: 10px; }div { background: #fff; border-radius: 5px; padding: 20px; margin-bottom: 10px; }h1 { color: #333; } p { color: #555; }form { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }label { font-weight: bold; }input[type='number'], select { width: 90%; padding: 5px; }input[type='submit'] { grid-column: 1 / -1; padding: 10px; background: #007bff; color: white; border: none; border-radius: 5px; font-size: 1em; } h3 { grid-column: 1 / -1; margin-top: 5px; margin-bottom: 5px; border-bottom: 1px solid #ccc; padding-bottom: 5px; }</style></head><body><h1>GRAVE Controller</h1>";
    
    
    // Div: Estado Atual e Informação do AP
    html += "<div><h2>Estado Atual</h2>";
    
    RTC.getTime(&RTCtime); 
    RTC.getDate(&RTCdate);
    
    sprintf(str_buffer, "<p>Hora RTC: <strong>%02d:%02d:%02d</strong> (Hora Local)</strong></p>",
             RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);
    html += str_buffer;
    sprintf(str_buffer, "<p>Data RTC: <strong>%02d/%02d/%04d</strong></p>",
             RTCdate.Date, RTCdate.Month, RTCdate.Year);
    html += str_buffer;
    
    // Exibe o estado do Alarme (implícito pelo LED)
    sprintf(str_buffer, "<p>AMP / MP3 Player: <strong>%s</strong> (Volume: %d)</p>",
             is_alarm_active ? "ON / Play" : "OFF / Stop", alarmConfig.volume);
    html += str_buffer;
    
    html += "</div>";

    // --- NOVO DIV: CONTROLE DE VOLUME MP3 ---
    html += "<div><h2>Controle de Volume do MP3</h2>";
    html += "<p>Ajuste o volume (0-30). O volume atual é: <strong>" + String(alarmConfig.volume) + "</strong>.</p>";
    html += "<form action='/setvolume' method='POST' style='grid-template-columns: 1fr;'>";
    
    html += "<label for='volume'>Nível de Volume:</label>";
    html += "<input type='range' id='volume' name='v' min='0' max='30' value='" + String(alarmConfig.volume) + "' style='width: 95%; margin-top: 5px; margin-bottom: 15px;'>";
    html += "<input type='submit' value='Salvar Volume' style='grid-column: 1 / -1; margin-top: 0;'>";
    html += "</form></div>";
    // ----------------------------------------


    // Div: Definir Alarme (para múltiplos períodos)
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
    
    // Loop para gerar os 3 conjuntos de campos (MAX_PERIODS = 3)
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

    // --- NOVO DIV: SINCRONIZAÇÃO MANUAL DO RTC ---
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
    if (server.method() == HTTP_POST) {
        
        AlarmData newConfig = alarmConfig; // Copia a configuração atual, incluindo o volume
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

            start_h = constrain(start_h, 0, 23);
            start_m = constrain(start_m, 0, 59);
            end_h = constrain(end_h, 0, 23);
            end_m = constrain(end_m, 0, 59);

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

        Serial.printf("[Servidor Web] %d períodos ativos definidos.\n", alarmConfig.num_periods);
        
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Metodo nao permitido");
    }
}

// --- NOVO HANDLER PARA DEFINIR O VOLUME ---
void handleSetVolume() {
    if (server.method() == HTTP_POST) {
        
        int new_volume = server.arg("v").toInt();
        new_volume = constrain(new_volume, 0, 30); 
        
        if (alarmConfig.volume != new_volume) {
            alarmConfig.volume = new_volume;
            mp3.setVolume(new_volume);
            saveAlarmConfig(); 
            Serial.printf("[Servidor Web] Volume MP3 ajustado para: %d\n", new_volume);
        }

        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Metodo nao permitido");
    }
}
// ----------------------------------------

void handleSetTime() {
    if (server.method() == HTTP_POST) {
        
        // Obter e validar a Hora
        int new_h = constrain(server.arg("h").toInt(), 0, 23);
        int new_m = constrain(server.arg("m").toInt(), 0, 59);
        int new_s = constrain(server.arg("s").toInt(), 0, 59);
        
        // Obter e validar a Data
        int new_d = constrain(server.arg("d").toInt(), 1, 31);
        int new_mon = constrain(server.arg("mon").toInt(), 1, 12);
        int new_y = constrain(server.arg("y").toInt(), 2024, 2100); 

        // Definir a Hora no RTC
        RTCtime.Hours = new_h;
        RTCtime.Minutes = new_m;
        RTCtime.Seconds = new_s;
        RTC.setTime(&RTCtime);
        
        // Definir a Data no RTC
        RTCdate.Date = new_d;
        RTCdate.Month = new_mon;
        RTCdate.Year = new_y;
        RTC.setDate(&RTCdate);
        
        Serial.printf("[Servidor Web] RTC Ajustado para: %02d/%02d/%04d %02d:%02d:%02d\n",
                      new_d, new_mon, new_y, new_h, new_m, new_s);

        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(405, "text/plain", "Metodo nao permitido");
    }
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Nao encontrado");
}

// --- Funções Principais (Setup e Loop) ---

void setup() {
    Serial.begin(115200); 
    
    AtomS3.begin(true); 
    AtomS3.dis.setBrightness(100);
    
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    Serial.println("M5Atom S3 RTC Controller a iniciar...");
    RTC.begin(); 
    
    // Configuração do MP3 Player
    mp3 = YX5300_ESP32(Serial1, MP3_RX_PIN, MP3_TX_PIN);
    mp3.enableDebugging();
    
    // Define o pino como saída e o estado inicial como INATIVO (HIGH)
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(OUTPUT_PIN, HIGH); 

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("ERRO FATAL: Falha ao inicializar EEPROM.");
        while(true); 
    }
    
    loadAlarmConfig();
    
    // NOVO: Define o volume inicial do MP3 Player
    mp3.setVolume(alarmConfig.volume);
    Serial.printf("[MP3] Volume inicial definido para: %d\n", alarmConfig.volume);

    setupAPMode(); 
    
    // --- ROTAS DO SERVIDOR WEB ---
    server.on("/", HTTP_GET, handleRoot);     
    server.on("/set", HTTP_POST, handleSet);   
    server.on("/settime", HTTP_POST, handleSetTime); 
    server.on("/setvolume", HTTP_POST, handleSetVolume); // NOVA ROTA PARA VOLUME
    server.onNotFound(handleNotFound);        
    server.begin();
    
    Serial.println("Servidor Web iniciado em Modo AP.");
    Serial.printf("Acesse http://%s\n", AP_IP.toString().c_str());
    
    RTC.getTime(&RTCtime);
    RTC.getDate(&RTCdate);
    
    // Força a verificação do estado do alarme para definir a cor inicial correta (Verde ou Vermelho)
    checkAlarmState(); 
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        AtomS3.update(); 
        
        RTC.getTime(&RTCtime);
        RTC.getDate(&RTCdate); 
        checkAlarmState(); 
    }
    
    delay(1); 
}
