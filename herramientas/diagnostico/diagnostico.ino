/*
 * diagnostico.ino — Prueba de hardware del robot balanceador
 *
 * Sketch independiente: NO modifica el firmware del robot. Se carga en lugar
 * del firmware, se usa desde el Serial Monitor (115200 baud) y después se
 * vuelve a cargar esp32_balancing_robot.ino. Se puede borrar la carpeta
 * herramientas/diagnostico/ cuando ya no haga falta.
 *
 * Qué funciona alimentando solo por USB (sin baterías):
 *   - ESP32, WiFi, MPU-6050, LEDs y buzzer (según cómo esté cableado el shield).
 *   - Los motores NO se mueven: los A4988 necesitan VMOT (8-35 V) de la batería.
 *     Para probarlos sin baterías, usar una fuente de 7-8.4 V con límite de
 *     corriente (~1.5 A) en la entrada de batería.
 *
 * Pines: los mismos de esp32_balancing_robot/defines.h.
 */

#include <Wire.h>
#include <WiFi.h>

// --- Pines (copiados de defines.h) ---
const int PIN_ENABLE_MOTORS = 12;   // Activo en LOW
const int PIN_MOTOR1_DIR    = 27;
const int PIN_MOTOR1_STEP   = 14;
const int PIN_MOTOR2_DIR    = 25;
const int PIN_MOTOR2_STEP   = 26;
const int PIN_LED           = 32;
const int PIN_WIFI_LED      = 2;
const int PIN_BUZZER        = 33;

const uint8_t MPU_ADDR = 0x68;
const int MICROSTEPPING = 8;                       // Igual que defines.h
const int STEPS_PER_REV = 200 * MICROSTEPPING;     // NEMA17 de 1.8°

// ============================================================================
// Utilidades
// ============================================================================
void titulo(const char *t) {
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println(t);
  Serial.println("----------------------------------------");
}

void resultado(bool ok, const char *msg) {
  Serial.print(ok ? "  [OK]    " : "  [FALLA] ");
  Serial.println(msg);
}

bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuRead(uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// Lee aceleración (g) y giro (°/s) con la configuración por defecto (±2 g, ±250 °/s)
bool mpuSample(float a[3], float g[3]) {
  uint8_t b[14];
  if (!mpuRead(0x3B, b, 14)) return false;
  for (int i = 0; i < 3; i++) {
    a[i] = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]) / 16384.0f;
    g[i] = (int16_t)(b[8 + 2 * i] << 8 | b[8 + 2 * i + 1]) / 131.0f;
  }
  return true;
}

bool hayTecla() {
  if (!Serial.available()) return false;
  while (Serial.available()) Serial.read();
  return true;
}

// ============================================================================
// 1. Bus I2C y MPU-6050
// ============================================================================
void probarMPU() {
  titulo("1. Bus I2C y MPU-6050 (dejar el robot quieto)");

  int encontrados = 0;
  Serial.print("  Dispositivos I2C:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
      encontrados++;
    }
  }
  Serial.println(encontrados ? "" : " ninguno");

  uint8_t who = 0;
  if (!mpuRead(0x75, &who, 1)) {
    resultado(false, "El MPU-6050 no responde en 0x68. Revisar SDA (21), SCL (22), VCC y GND.");
    return;
  }
  char msg[96];
  if (who == 0x68) {
    resultado(true, "WHO_AM_I = 0x68 (MPU-6050 original)");
  } else {
    snprintf(msg, sizeof(msg), "WHO_AM_I = 0x%02X: responde, pero es un clon u otro modelo", who);
    resultado(true, msg);
  }

  mpuWrite(0x6B, 0x00);  // Despertar
  delay(100);

  const int N = 200;
  float sa[3] = {0}, sg[3] = {0}, sg2[3] = {0};
  for (int i = 0; i < N; i++) {
    float a[3], g[3];
    if (!mpuSample(a, g)) {
      resultado(false, "Error leyendo datos del sensor (cable I2C flojo?)");
      return;
    }
    for (int k = 0; k < 3; k++) { sa[k] += a[k]; sg[k] += g[k]; sg2[k] += g[k] * g[k]; }
    delay(5);
  }
  float ax = sa[0] / N, ay = sa[1] / N, az = sa[2] / N;
  float modulo = sqrtf(ax * ax + ay * ay + az * az);
  snprintf(msg, sizeof(msg), "Acelerómetro: |a| = %.2f g (esperado ~1.00 g)  [x=%.2f y=%.2f z=%.2f]",
           modulo, ax, ay, az);
  resultado(modulo > 0.85f && modulo < 1.15f, msg);

  bool gyroOk = true;
  for (int k = 0; k < 3; k++) {
    float media = sg[k] / N;
    float ruido = sqrtf(fmaxf(sg2[k] / N - media * media, 0));
    const char eje = "XYZ"[k];
    snprintf(msg, sizeof(msg), "Giroscopio %c: offset %.2f °/s, ruido %.2f °/s (quieto: offset < 10, ruido < 1)",
             eje, media, ruido);
    bool ok = fabsf(media) < 10.0f && ruido < 1.0f;
    gyroOk &= ok;
    resultado(ok, msg);
  }
  if (!gyroOk) Serial.println("  Si el robot se movió durante la prueba, repetirla con el robot quieto.");
  Serial.println("  Para ver si el giroscopio responde al movimiento, usar la opción 5.");
}

// ============================================================================
// 2. Motores (necesitan VMOT: batería o fuente externa)
// ============================================================================
void moverMotor(int pinStep, int pinDir, bool adelante, int pasos) {
  digitalWrite(pinDir, adelante ? HIGH : LOW);
  for (int i = 0; i < pasos; i++) {
    digitalWrite(pinStep, HIGH);
    delayMicroseconds(5);
    digitalWrite(pinStep, LOW);
    delayMicroseconds(600);  // ~1600 pasos/s: media vuelta en ~0.5 s
  }
}

void probarMotores() {
  titulo("2. Motores (sin VMOT no se mueven: es normal por USB)");
  Serial.println("  Cada motor da media vuelta hacia adelante y vuelve.");
  Serial.println("  Mirar las ruedas: ambas deben girar suave, sin saltos ni zumbidos raros.");
  digitalWrite(PIN_ENABLE_MOTORS, LOW);  // Habilitar drivers
  delay(50);

  Serial.println("  Motor 1 (rueda en STEP 14 / DIR 27)...");
  moverMotor(PIN_MOTOR1_STEP, PIN_MOTOR1_DIR, true, STEPS_PER_REV / 2);
  delay(300);
  moverMotor(PIN_MOTOR1_STEP, PIN_MOTOR1_DIR, false, STEPS_PER_REV / 2);
  delay(500);

  Serial.println("  Motor 2 (rueda en STEP 26 / DIR 25)...");
  moverMotor(PIN_MOTOR2_STEP, PIN_MOTOR2_DIR, true, STEPS_PER_REV / 2);
  delay(300);
  moverMotor(PIN_MOTOR2_STEP, PIN_MOTOR2_DIR, false, STEPS_PER_REV / 2);

  digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Deshabilitar
  Serial.println("  Listo. Si con VMOT un motor no gira o vibra sin avanzar: revisar el");
  Serial.println("  cable del motor, el driver A4988 y el ajuste de corriente (potenciómetro).");
}

// ============================================================================
// 3. LEDs y buzzer
// ============================================================================
void probarIndicadores() {
  titulo("3. LEDs y buzzer");
  Serial.println("  LED de la placa (GPIO 2): 3 parpadeos");
  for (int i = 0; i < 3; i++) { digitalWrite(PIN_WIFI_LED, HIGH); delay(200); digitalWrite(PIN_WIFI_LED, LOW); delay(200); }
  Serial.println("  LEDs brillantes (GPIO 32): 3 parpadeos");
  for (int i = 0; i < 3; i++) { digitalWrite(PIN_LED, HIGH); delay(200); digitalWrite(PIN_LED, LOW); delay(200); }
  Serial.println("  Buzzer (GPIO 33): 2 pitidos");
  for (int i = 0; i < 2; i++) { digitalWrite(PIN_BUZZER, HIGH); delay(120); digitalWrite(PIN_BUZZER, LOW); delay(150); }
  Serial.println("  Si algo no prendió o no sonó, puede necesitar la batería según el cableado.");
}

// ============================================================================
// 4. WiFi
// ============================================================================
void probarWiFi() {
  titulo("4. Radio WiFi (búsqueda de redes)");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    resultado(false, "No se encontró ninguna red (o falló la radio)");
  } else {
    char msg[64];
    snprintf(msg, sizeof(msg), "%d redes encontradas", n);
    resultado(true, msg);
    for (int i = 0; i < n && i < 5; i++) {
      Serial.printf("    %-28s %4d dBm\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

// ============================================================================
// 5. Ángulo en vivo
// ============================================================================
void anguloEnVivo() {
  titulo("5. Ángulo en vivo (inclinar el robot; cualquier tecla + Enter para salir)");
  mpuWrite(0x6B, 0x00);
  float angulo = 0;
  bool primero = true;
  unsigned long t = micros(), ultimoPrint = 0;
  while (!hayTecla()) {
    float a[3], g[3];
    if (!mpuSample(a, g)) { Serial.println("  Error de lectura I2C"); delay(500); continue; }
    unsigned long ahora = micros();
    float dt = (ahora - t) * 1e-6f;
    t = ahora;
    // Mismos ejes que el firmware (MPU6050.cpp): ángulo = atan2(acel Y, acel Z), giro X
    float angAcc = atan2f(a[1], a[2]) * 57.2958f;
    angulo = primero ? angAcc : 0.98f * (angulo + g[0] * dt) + 0.02f * angAcc;
    primero = false;
    if (millis() - ultimoPrint > 100) {
      ultimoPrint = millis();
      Serial.printf("  ángulo %7.1f°   (acel %7.1f°, giro X %7.1f °/s)\n", angulo, angAcc, g[0]);
    }
    delay(5);
  }
  Serial.println("  Fin.");
}

// ============================================================================
void menu() {
  Serial.println();
  Serial.println("=========== DIAGNÓSTICO DEL ROBOT ===========");
  Serial.println("  1  MPU-6050 (I2C, acelerómetro, giroscopio)");
  Serial.println("  2  Motores   (necesita batería o fuente)");
  Serial.println("  3  LEDs y buzzer");
  Serial.println("  4  WiFi");
  Serial.println("  5  Ángulo en vivo");
  Serial.println("  a  Todo (1 a 4)");
  Serial.println("Escribí una opción y Enter:");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_ENABLE_MOTORS, OUTPUT);
  digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Motores deshabilitados al arrancar
  pinMode(PIN_MOTOR1_DIR, OUTPUT);
  pinMode(PIN_MOTOR1_STEP, OUTPUT);
  pinMode(PIN_MOTOR2_DIR, OUTPUT);
  pinMode(PIN_MOTOR2_STEP, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_WIFI_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_WIFI_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin();            // SDA 21, SCL 22
  Wire.setClock(400000);

  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available()) Serial.read();  // Descartar Enter y demás
  switch (c) {
    case '1': probarMPU(); break;
    case '2': probarMotores(); break;
    case '3': probarIndicadores(); break;
    case '4': probarWiFi(); break;
    case '5': anguloEnVivo(); break;
    case 'a': case 'A':
      probarMPU(); probarMotores(); probarIndicadores(); probarWiFi();
      break;
    default: return;
  }
  menu();
}
