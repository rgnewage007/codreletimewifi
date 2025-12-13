#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Pines para ESP32-S3
int ledPin = 14;        // Pin para el LED principal de control
int ledIndicator = 48;  // Pin para el LED indicador rojo

WiFiClient client;

// Constantes para tiempos
const int SEGUNDOS_EN_UNA_HORA = 3600;
const int SEGUNDOS_EN_UN_DIA = 86400;
const int DESFASE_HORARIO = 6 * SEGUNDOS_EN_UNA_HORA; // UTC-6

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== IOTdoor ESP32-S3 ===");
    
    pinMode(ledPin, OUTPUT);
    pinMode(ledIndicator, OUTPUT);
    
    // APAGAR ambos LEDs al inicio
    digitalWrite(ledPin, LOW);
    digitalWrite(ledIndicator, LOW);
    
    // SECUENCIA DE PRUEBA DEL LED PRINCIPAL
    Serial.println("Probando LED principal...");
    for (int i = 0; i < 1; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            digitalWrite(ledPin, HIGH);
            delay(500);
            digitalWrite(ledPin, LOW);
            delay(500);
        }
        delay(1000);
    }
    
    // CONEXIÓN WIFI
    Serial.println("\nConectando a WiFi...");
    WiFiManager wifiManager;
    
    // Configurar WiFiManager
    wifiManager.setTimeout(180);
    wifiManager.setConnectTimeout(30);
    
    if(!wifiManager.autoConnect("IOTdoor")) {
        Serial.println("ERROR: Fallo en conexión WiFi");
        // Parpadeo rápido de error
        for(int i=0; i<15; i++) {
            digitalWrite(ledPin, !digitalRead(ledPin));
            delay(200);
        }
        ESP.restart();
    }
    
    Serial.println("✅ WiFi conectado!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    
    // Test de conexión a internet
    Serial.println("Probando conexión a internet...");
    if(testInternetConnection()) {
        Serial.println("✅ Internet disponible");
    } else {
        Serial.println("⚠️  Problemas con internet");
    }
    
    delay(500);
}

bool testInternetConnection() {
    HTTPClient http;
    http.begin("http://www.google.com");
    http.setTimeout(10000);
    int httpCode = http.GET();
    http.end();
    
    Serial.print("Test internet - Código: ");
    Serial.println(httpCode);
    
    return (httpCode == HTTP_CODE_OK);
}

void loop()
{
    Serial.println("\n--- Ciclo de lectura ---");
    
    // Señal visual con LED indicador
    for (int i = 0; i < 2; i++)
    {
        digitalWrite(ledIndicator, HIGH);
        delay(300);
        digitalWrite(ledIndicator, LOW);
        delay(300);
    }

    Serial.println("Obteniendo hora del servidor...");
    String horaActual = getTimeFromServer();
    
    if (!horaActual.isEmpty())
    {
        digitalWrite(ledIndicator, HIGH); // Encender indicador durante proceso
        
        int unixTime = horaActual.toInt();
        int segundosDelDiaUTC = unixTime % SEGUNDOS_EN_UN_DIA;
        int segundosDelDiaLocal = (segundosDelDiaUTC - DESFASE_HORARIO + SEGUNDOS_EN_UN_DIA) % SEGUNDOS_EN_UN_DIA;

        Serial.print("Segundos del día local: ");
        Serial.println(segundosDelDiaLocal);
       
        int horas = segundosDelDiaLocal / 3600;
        int minutos = (segundosDelDiaLocal % 3600) / 60;
        String horaFormato = String(horas) + ":" + (minutos < 10 ? "0" : "") + String(minutos);
        Serial.print("🕐 Hora local: ");
        Serial.println(horaFormato);
        
        // Lógica de encendido (noche: 6:40 PM a 6:40 AM)
        int inicioDia = 6 * SEGUNDOS_EN_UNA_HORA + 40 * 60;  // 6:40 AM
        int finDia = 18 * SEGUNDOS_EN_UNA_HORA + 40 * 60;    // 6:40 PM
        
        bool encender = (segundosDelDiaLocal < inicioDia || segundosDelDiaLocal >= finDia);

        if (encender)
        {
            digitalWrite(ledPin, HIGH); // Encender luz
            Serial.println("💡 Luz ENCENDIDA (noche)");
        }
        else
        {
            digitalWrite(ledPin, LOW); // Apagar luz
            Serial.println("🌞 Luz APAGADA (día)");
        }
        
        digitalWrite(ledIndicator, LOW);
    }
    else
    {
        Serial.println("❌ Error al obtener hora");
        // Parpadeo de error
        for(int i=0; i<3; i++) {
            digitalWrite(ledIndicator, HIGH);
            delay(150);
            digitalWrite(ledIndicator, LOW);
            delay(150);
        }
        return;
    }
    
    Serial.println("⏳ Esperando 30 minutos...");
    delay(1800000); // 30 minutos
}

String getTimeFromServer() {
    WiFiClient client;
    HTTPClient http;
    
    // Lista de servidores alternativos (intentar en orden)
    const char* servers[] = {
        "http://worldtimeapi.org/api/ip",
        "http://worldtimeapi.org/api/timezone/America/Mexico_City",
        "http://worldtimeapi.org/api/timezone/America/Chicago",
        "http://worldclockapi.com/api/json/utc/now"
    };
    
    int numServers = sizeof(servers) / sizeof(servers[0]);
    
    for(int attempt = 0; attempt < numServers; attempt++) {
        Serial.print("Intentando servidor ");
        Serial.print(attempt + 1);
        Serial.print(": ");
        Serial.println(servers[attempt]);
        
        http.setReuse(false);
        http.setTimeout(15000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        if(http.begin(client, servers[attempt])) {
            int httpCode = http.GET();
            Serial.print("HTTP Code: ");
            Serial.println(httpCode);
            
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                
                // Buscar unixtime en diferentes formatos
                int beginS = payload.indexOf("\"unixtime\":");
                if(beginS == -1) {
                    beginS = payload.indexOf("\"currentDateTime\":");
                    if(beginS != -1) {
                        // Formato de worldclockapi
                        beginS += 18;
                        int endS = payload.indexOf("\"", beginS);
                        String unixTimeString = payload.substring(beginS, endS);
                        Serial.print("Unix time alternativo: ");
                        Serial.println(unixTimeString);
                        http.end();
                        return unixTimeString;
                    }
                } else {
                    beginS += 11;
                    int endS = payload.indexOf(",", beginS);
                    String unixTimeString = payload.substring(beginS, endS);
                    Serial.print("Unix time: ");
                    Serial.println(unixTimeString);
                    http.end();
                    return unixTimeString;
                }
            }
            http.end();
            delay(2000); // Esperar entre intentos
        }
    }
    
    Serial.println("❌ Todos los servidores fallaron");
    return "";
}
