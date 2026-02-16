#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Cliente NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -21600, 60000); // -21600 = UTC-6, actualiza cada 60 segundos

int ledPin = 14;  // D5
int ledPin7 = 16; // D0

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    pinMode(ledPin7, OUTPUT);
    
    digitalWrite(ledPin, HIGH); // Apagado inicial
    digitalWrite(ledPin7, LOW);
    
    // WiFiManager
    WiFiManager wifiManager;
    wifiManager.autoConnect("IOTdoor");
    
    Serial.println("WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // Iniciar NTP
    timeClient.begin();
    timeClient.setTimeOffset(-21600); // UTC-6 en segundos
    
    // Esperar primera sincronización
    Serial.println("Sincronizando hora...");
    while(!timeClient.update()) {
        timeClient.forceUpdate();
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nHora sincronizada!");
}

void loop() {
    timeClient.update(); // Actualizar hora
    
    Serial.println("\n--- Ciclo de lectura ---");
    
    // Señal visual
    for (int i = 0; i < 2; i++) {
        digitalWrite(ledPin7, HIGH);
        delay(300);
        digitalWrite(ledPin7, LOW);
        delay(300);
    }
    
    // Obtener hora actual
    time_t rawTime = timeClient.getEpochTime();
    struct tm * timeInfo = localtime(&rawTime);
    
    int horas = timeInfo->tm_hour;
    int minutos = timeInfo->tm_min;
    
    Serial.print("🕐 Hora local: ");
    Serial.print(horas);
    Serial.print(":");
    Serial.println(minutos < 10 ? "0" + String(minutos) : String(minutos));
    
    // Lógica de encendido (6:40 PM a 6:40 AM)
    bool encender = (horas >= 18 && horas < 6) || 
                    (horas == 18 && minutos >= 40) || 
                    (horas < 6) || 
                    (horas == 6 && minutos < 40);
    
    if (encender) {
        digitalWrite(ledPin, LOW); // Encender
        Serial.println("💡 Luz APAGADA");
    } else {
        digitalWrite(ledPin, HIGH); // Apagar
        Serial.println("🌞 Luz ENCENDIDA");
    }
    
    delay(1800000); // 30 minutos
}
