#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>


int ledPin = 15;  // D8
int ledPin7 = 16; // D0

WiFiClient client;

// Constantes para tiempo
const int SEGUNDOS_EN_UNA_HORA = 3600;
const int SEGUNDOS_EN_UN_DIA = 86400; // 24 * 3600
const int DESFASE_HORARIO = 6 * SEGUNDOS_EN_UNA_HORA; // UTC-6 en segundos

void setup()
{
    pinMode(ledPin, OUTPUT);
    pinMode(ledPin7, OUTPUT);

    Serial.begin(9600);
    WiFiManager wifiManager;
    wifiManager.autoConnect("IOTdoor");
    delay(500);
}

void loop()
{
    // Señal visual de inicio de lectura
    for (int i = 1; i <= 3; i++)
    {
        digitalWrite(ledPin7, LOW);
        delay(500);
        digitalWrite(ledPin7, HIGH);
        delay(500);
    }

    Serial.println("Instrucción de lectura enviada.");
    String horaActual = getTime();
    if (!horaActual.isEmpty())
    {
        // Convertir hora actual a segundos del día en UTC
        int unixTime = horaActual.toInt();
        int segundosDelDiaUTC = (unixTime % SEGUNDOS_EN_UN_DIA + SEGUNDOS_EN_UN_DIA) % SEGUNDOS_EN_UN_DIA; // Obtener segundos del día UTC
        int segundosDelDiaLocal = (segundosDelDiaUTC - DESFASE_HORARIO + SEGUNDOS_EN_UN_DIA) % SEGUNDOS_EN_UN_DIA; // Ajustar a hora local

        Serial.println("Segundos del día en hora local:");
        Serial.println(segundosDelDiaLocal);
       

        int horas = segundosDelDiaLocal / 3600;  // División entera para obtener horas
int segundosRestantes = segundosDelDiaLocal % 3600;  // El resto para los segundos sobrantes
int minutos = segundosRestantes / 60;
String horaFormato = String(horas) + ":" + (minutos < 10 ? "0" : "") + String(minutos);
Serial.println(horaFormato);
        // Definir los intervalos basados en los segundos del día
        int inicioApagadoNoche = 0;
        int finApagadoNoche = 4 * SEGUNDOS_EN_UNA_HORA + 0 * 60; // 4:00
        int inicioEncendidoManana = finApagadoNoche;
        int finEncendidoManana = 6 * SEGUNDOS_EN_UNA_HORA + 40 * 60; // 6:40 
        int inicioApagadoDia = finEncendidoManana;
        int finApagadoDia = 18 * SEGUNDOS_EN_UNA_HORA + 40 * 60; // 18:40 
        int inicioEncendidoTarde = finApagadoDia;
        int finEncendidoTarde = 24 * SEGUNDOS_EN_UNA_HORA; // 11:59:59 PM (final del día)

        bool encender = false;

        // Aplicar la lógica según el intervalo actual
        if ((segundosDelDiaLocal >= inicioEncendidoManana && segundosDelDiaLocal < finEncendidoManana) || 
            (segundosDelDiaLocal >= inicioEncendidoTarde && segundosDelDiaLocal < finEncendidoTarde))
        {
            encender = true;
        }

        // Controlar el estado de la luz según la variable "encender"
        if (encender)
        {
            digitalWrite(ledPin, LOW); // Encender luz
            Serial.println("Luz encendida.");
        }
        else
        {
            digitalWrite(ledPin, HIGH); // Apagar luz
            Serial.println("Luz apagada.");
        }
    }
    delay(60000); // Esperar antes de volver a consultar la hora
}

String getTime()
{
    WiFiClient client;
    HTTPClient http;
    String timeS = "";

    http.begin(client, "http://worldtimeapi.org/api/ip");
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
        String payload = http.getString();

        int beginS = payload.indexOf("unixtime");
        int endS = payload.indexOf(",", beginS);

        if (beginS != -1 && endS != -1)
        {
            String unixTimeString = payload.substring(beginS + 10, endS); // Extraemos el valor de Unix time
            Serial.println("Unix time: " + unixTimeString);
            return unixTimeString;
        }
    }
    return "";
}