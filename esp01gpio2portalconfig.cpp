#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

// ========== CONFIGURACIÓN EEPROM ==========
#define EEPROM_SIZE 512
#define MAGIC_NUMBER 0xDEADBEEF  // Para validar datos guardados

struct HorarioConfig {
  unsigned long magic;      // Validación (0xDEADBEEF)
  int horaEncendido;        // Hora de encendido (0-23)
  int minutoEncendido;      // Minuto de encendido (0-59)
  int horaApagado;          // Hora de apagado (0-23)
  int minutoApagado;        // Minuto de apagado (0-59)
  bool configurado;         // Si ya se configuró alguna vez
};

HorarioConfig config;

// ========== VARIABLES GLOBALES ==========
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -21600, 60000); // UTC-6 (Costa Rica)

// Pines ESP-01
int ledPin = 2;      // GPIO2 - Luz principal
int ledPin7 = 1;     // GPIO1 (TX) - LED indicador

bool estadoLuz = false;
bool horaSincronizada = false;

// Control del portal temporal
unsigned long tiempoInicio = 0;
bool portalActivo = true;
unsigned long tiempoPortalCierre = 180000; // 3 minutos (180,000 ms)

// Intervalos
unsigned long ultimoChequeo = 0;
const unsigned long intervaloChequeo = 60000; // 1 minuto

// Web server para el portal de configuración
ESP8266WebServer server(80);

// ========== FUNCIONES EEPROM ==========
void cargarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  
  // Verificar si hay datos válidos
  if (config.magic != MAGIC_NUMBER || !config.configurado) {
    // Configuración por defecto: 18:40 encendido, 06:40 apagado
    Serial.println("📝 No hay configuración guardada, usando valores por defecto");
    config.magic = MAGIC_NUMBER;
    config.horaEncendido = 18;
    config.minutoEncendido = 40;
    config.horaApagado = 6;
    config.minutoApagado = 40;
    config.configurado = false;
    guardarConfiguracion();
  } else {
    Serial.println("✅ Configuración cargada desde EEPROM");
    Serial.print("   Encendido: ");
    Serial.print(config.horaEncendido);
    Serial.print(":");
    Serial.println(config.minutoEncendido);
    Serial.print("   Apagado: ");
    Serial.print(config.horaApagado);
    Serial.print(":");
    Serial.println(config.minutoApagado);
  }
}

void guardarConfiguracion() {
  EEPROM.put(0, config);
  EEPROM.commit();
  Serial.println("💾 Configuración guardada en EEPROM");
}

// ========== PÁGINA WEB DE CONFIGURACIÓN ==========
 
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f0f0f0;}";
  html += ".container{max-width:500px;margin:auto;background:white;padding:20px;border-radius:10px;}";
  html += "h1{color:#333;text-align:center;}";
  html += "label{font-weight:bold;margin-top:10px;display:block;}";
  html += "input{width:100%;padding:8px;margin:5px 0 15px 0;border:1px solid #ccc;border-radius:4px;}";
  html += "button{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;}";
  html += "button:hover{background:#45a049;}";
  html += ".info{background:#e7f3ff;padding:10px;border-radius:5px;margin-top:20px;font-size:14px;}";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Configurar Horarios</h1>";
  
  html += "<form action='/guardar' method='POST'>";
  html += "<label>Hora ENCENDIDO (0-23):</label>";
  html += "<input type='number' name='horaEnc' min='0' max='23' value='" + String(config.horaEncendido) + "' required>";
  html += "<label>Minuto ENCENDIDO (0-59):</label>";
  html += "<input type='number' name='minEnc' min='0' max='59' value='" + String(config.minutoEncendido) + "' required>";
  
  html += "<label>Hora APAGADO (0-23):</label>";
  html += "<input type='number' name='horaApa' min='0' max='23' value='" + String(config.horaApagado) + "' required>";
  html += "<label>Minuto APAGADO (0-59):</label>";
  html += "<input type='number' name='minApa' min='0' max='59' value='" + String(config.minutoApagado) + "' required>";
  
  html += "<button type='submit'>Guardar Configuracion</button>";
  html += "</form>";
  
  html += "<div class='info'>";
  html += "<strong>Horario actual:</strong><br>";
  html += "ENCENDIDO: " + String(config.horaEncendido) + ":" + (config.minutoEncendido < 10 ? "0" : "") + String(config.minutoEncendido) + "<br>";
  html += "APAGADO: " + String(config.horaApagado) + ":" + (config.minutoApagado < 10 ? "0" : "") + String(config.minutoApagado);
  html += "<br><br>[WARNING] Este portal solo esta disponible por 3 minutos despues del reinicio.";
  html += "</div>";
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleGuardar() {
  if (server.hasArg("horaEnc") && server.hasArg("minEnc") && 
      server.hasArg("horaApa") && server.hasArg("minApa")) {
    
    config.horaEncendido = server.arg("horaEnc").toInt();
    config.minutoEncendido = server.arg("minEnc").toInt();
    config.horaApagado = server.arg("horaApa").toInt();
    config.minutoApagado = server.arg("minApa").toInt();
    config.configurado = true;
    
    guardarConfiguracion();
    
    String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='3;url=/'>";
    html += "<style>body{text-align:center;margin-top:50px;font-family:Arial;}</style>";
    html += "</head><body>";
    html += "<h2>[OK] Configuracion Guardada!</h2>";
    html += "<p>Redirigiendo en 3 segundos...</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    
    Serial.println("Nueva configuracion guardada via web");
  } else {
    server.send(400, "text/plain", "Error: Datos incompletos");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// ========== INICIAR PORTAL TEMPORAL ==========
void iniciarPortalConfiguracion() {
  Serial.println("🌐 Iniciando portal de configuración temporal...");
  
  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/guardar", HTTP_POST, handleGuardar);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("✅ Servidor web iniciado");
  Serial.print("📱 Conéctate a la red 'IOTdoor' y visita http://192.168.4.1");
}

// ========== FUNCIONES PRINCIPALES ==========
void sincronizarHora() {
  Serial.println("Sincronizando hora...");
  unsigned long inicio = millis();
  
  while (!timeClient.update()) {
    if (millis() - inicio > 10000) {
      Serial.println("❌ No se pudo sincronizar NTP");
      horaSincronizada = false;
      return;
    }
    delay(500);
    Serial.print(".");
  }
  
  unsigned long epochTime = timeClient.getEpochTime();
  unsigned long segundosDia = epochTime % 86400UL;
  
  int horas = segundosDia / 3600;
  int minutos = (segundosDia % 3600) / 60;
  int segundos = segundosDia % 60;
  
  Serial.println("\n✅ HORA SINCRONIZADA!");
  Serial.print("🕒 Hora actual: ");
  Serial.print(horas < 10 ? "0" : "");
  Serial.print(horas);
  Serial.print(":");
  Serial.print(minutos < 10 ? "0" : "");
  Serial.print(minutos);
  Serial.print(":");
  Serial.print(segundos < 10 ? "0" : "");
  Serial.println(segundos);
  
  horaSincronizada = true;
}

void actualizarEstadoLuz() {
  if (!horaSincronizada || !config.configurado) {
    return;
  }
  
  unsigned long epochTime = timeClient.getEpochTime();
  unsigned long segundosDia = epochTime % 86400UL;
  
  int horas = segundosDia / 3600;
  int minutos = (segundosDia % 3600) / 60;
  
  // Calcular si debe estar encendido usando horarios configurables
  bool debeEstarEncendida = false;
  
  // Convertir horario actual a minutos desde medianoche
  int minutosActuales = horas * 60 + minutos;
  int minutosEncendido = config.horaEncendido * 60 + config.minutoEncendido;
  int minutosApagado = config.horaApagado * 60 + config.minutoApagado;
  
  if (minutosEncendido < minutosApagado) {
    // Horario normal (ej: 18:40 a 06:40)
    debeEstarEncendida = (minutosActuales >= minutosEncendido || minutosActuales < minutosApagado);
  } else {
    // Horario que cruza medianoche
    debeEstarEncendida = (minutosActuales >= minutosEncendido || minutosActuales < minutosApagado);
  }
  
  if (debeEstarEncendida != estadoLuz) {
    estadoLuz = debeEstarEncendida;
    digitalWrite(ledPin, estadoLuz ? HIGH : LOW);
    
    Serial.print("⚡ CAMBIO DE ESTADO - ");
    Serial.print(horas);
    Serial.print(":");
    Serial.print(minutos);
    Serial.print(" - Luz: ");
    Serial.println(estadoLuz ? "ENCENDIDA" : "APAGADA");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n🚀 Iniciando sistema programable ESP-01...");
  Serial.println("========================================");
  
  // Cargar configuración de horarios desde EEPROM
  cargarConfiguracion();
  
  // Configurar pines
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin7, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(ledPin7, LOW);
  
  tiempoInicio = millis();
  ultimoChequeo = millis();
  
  // Configurar WiFiManager (modo normal)
  WiFiManager wifiManager;
  wifiManager.setTimeout(60);
  
  Serial.println("Conectando a WiFi...");
  if (!wifiManager.autoConnect("IOTdoor")) {
    Serial.println("⚠️ No se pudo conectar al WiFi");
  } else {
    Serial.println("✅ WiFi conectado!");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());
  }
  
  // Iniciar portal temporal para configuración de horarios
  iniciarPortalConfiguracion();
  
  // Iniciar NTP
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    sincronizarHora();
  }
  
  Serial.println("⚡ Sistema listo!");
  Serial.print("⏰ Portal de configuración activo por ");
  Serial.print(tiempoPortalCierre / 60000);
  Serial.println(" minutos");
}

void loop() {
  unsigned long ahora = millis();
  
  // Manejar el servidor web si el portal está activo
  if (portalActivo) {
    server.handleClient();
    
    // Cerrar portal después de 3 minutos
    if (ahora - tiempoInicio >= tiempoPortalCierre) {
      portalActivo = false;
      server.stop();
      Serial.println("🔒 Portal de configuración cerrado (tiempo expirado)");
      Serial.println("✅ Sistema funcionando con horarios configurados");
    }
  }
  
  // Lógica normal cada minuto
  if (ahora - ultimoChequeo >= intervaloChequeo) {
    ultimoChequeo = ahora;
    
    if (WiFi.status() == WL_CONNECTED) {
      timeClient.update();
      if (!horaSincronizada) {
        sincronizarHora();
      }
      actualizarEstadoLuz();
    }
    
    // LED indicador de vida
    digitalWrite(ledPin7, !digitalRead(ledPin7));
  }
  
  delay(100);
}
