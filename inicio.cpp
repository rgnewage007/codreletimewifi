#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -21600, 60000); // UTC-6 (Costa Rica)

// Pines NodeMCU
int ledPin = 14;   // D5 - GPIO14 - Luz principal
int ledPin7 = 16;  // D0 - GPIO16 - LED indicador

bool estadoLuz = false;
bool horaSincronizada = false;

// 🔄 Reinicio cada 72 horas
unsigned long tiempoInicio = 0;
unsigned long ultimoChequeo = 0;
const unsigned long intervaloReinicio = 259200000UL; // 72h en ms
const unsigned long intervaloChequeo = 60000; // 1 minuto

// ----------------------------------------------------

void sincronizarHora() {
    Serial.println("Sincronizando hora...");
    unsigned long inicio = millis();
    
    while (!timeClient.update()) {
        if (millis() - inicio > 10000) { // máximo 10 segundos
            Serial.println("❌ No se pudo sincronizar NTP");
            horaSincronizada = false;
            return;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\n✅ Hora sincronizada!");
    horaSincronizada = true;
}

// ----------------------------------------------------

void actualizarEstadoLuz() {
    if (!horaSincronizada) {
        Serial.println("⏳ Hora no sincronizada - esperando...");
        return;
    }
    
    // 🔥 CORREGIDO: NTPClient ya aplica el offset UTC-6
    unsigned long epochTime = timeClient.getEpochTime();
    unsigned long segundosDia = epochTime % 86400UL;
    
    int horas = segundosDia / 3600;
    int minutos = (segundosDia % 3600) / 60;
    int segundos = segundosDia % 60;
    
    Serial.printf("Hora local: %02d:%02d:%02d\n", horas, minutos, segundos);
    
    // Lógica de encendido: 6:40 PM a 6:40 AM
    bool debeEstarEncendida = 
        (horas > 18 || (horas == 18 && minutos >= 40)) ||
        (horas < 6  || (horas == 6  && minutos < 40));
    
    // ✅ SOLO escribir al pin si cambia el estado
    if (debeEstarEncendida != estadoLuz) {
        estadoLuz = debeEstarEncendida;
        
        // NodeMCU: LOW = encendido, HIGH = apagado
        digitalWrite(ledPin, estadoLuz ? LOW : HIGH);
        
        if (estadoLuz) {
            Serial.println("💡 LUZ ENCENDIDA (6:40 PM - 6:40 AM)");
        } else {
            Serial.println("🌞 LUZ APAGADA (6:40 AM - 6:40 PM)");
        }
    }
}

// ----------------------------------------------------

void verificarReinicio() {
    if (millis() - tiempoInicio >= intervaloReinicio) {
        Serial.println("🔄 Reinicio automático por mantenimiento (72 horas)");
        delay(1000);
        ESP.restart();
    }
}

// ----------------------------------------------------

void verificarWiFi() {
    static unsigned long ultimoIntentoWiFi = 0;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - ultimoIntentoWiFi > 300000) { // Reintentar cada 5 minutos
            ultimoIntentoWiFi = millis();
            Serial.println("📡 WiFi desconectado - intentando reconectar...");
            WiFi.reconnect();
        }
    }
}

// ----------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n🚀 Iniciando sistema...");
    
    pinMode(ledPin, OUTPUT);
    pinMode(ledPin7, OUTPUT);
    
    // Estado inicial
    digitalWrite(ledPin, HIGH);   // Apagado
    digitalWrite(ledPin7, LOW);   // Apagado
    
    tiempoInicio = millis();
    ultimoChequeo = millis();
    
    // Configuración WiFi
    WiFiManager wifiManager;
    wifiManager.setTimeout(60);
    wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
        Serial.println("📱 Modo AP activado - Conéctate a 'IOTdoor' para configurar");
    });
    
    Serial.println("Conectando a WiFi...");
    if (!wifiManager.autoConnect("IOTdoor")) {
        Serial.println("⚠️ No se pudo conectar al WiFi - continuando sin conexión");
    } else {
        Serial.println("✅ WiFi conectado!");
        Serial.print("🌐 IP: ");
        Serial.println(WiFi.localIP());
    }
    
    // Iniciar NTP si hay WiFi
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.begin();
        sincronizarHora();
    }
    
    Serial.println("⚡ Sistema listo!");
}

// ----------------------------------------------------

void loop() {
    unsigned long ahora = millis();
    
    // Chequeo periódico cada minuto
    if (ahora - ultimoChequeo >= intervaloChequeo) {
        ultimoChequeo = ahora;
        
        // Mantener WiFi activo
        verificarWiFi();
        
        // Actualizar hora si hay conexión
        if (WiFi.status() == WL_CONNECTED) {
            timeClient.update();
            
            if (!horaSincronizada) {
                sincronizarHora();
            }
        }
        
        // Actualizar estado de la luz
        actualizarEstadoLuz();
        
        // Verificar reinicio programado
        verificarReinicio();
        
        // 💡 LED indicador de vida (D0 parpadea cada minuto)
        digitalWrite(ledPin7, !digitalRead(ledPin7));
        Serial.println("❤️ Sistema activo...");
    }
    
    // Pequeño delay para no saturar
    delay(100);
}
