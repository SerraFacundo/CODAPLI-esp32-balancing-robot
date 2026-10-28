/*
 * secrets.example.h
 *
 * Plantilla de claves. Copiar este archivo como secrets.h (en esta misma carpeta)
 * y completar los valores. secrets.h está en .gitignore: las claves reales no se
 * suben al repositorio. Sin secrets.h el firmware no compila (a propósito).
 */

#ifndef SECRETS_H_
#define SECRETS_H_

// Contraseña del AP WiFi del robot (WPA2, mínimo 8 caracteres).
#define SECRET_AP_PASSWORD "cambiar-esta-clave"

// Token que cada cliente debe enviar (T:<token>) antes de que se acepten comandos.
// Usar al menos 16 caracteres al azar; no puede contener ':' ni espacios.
#define SECRET_WS_TOKEN    "cambiar-este-token"

// Red de la laptop para el modo AP+STA (plataforma IIoT).
// Dejar SECRET_STA_SSID vacío ("") para usar solo el AP propio del robot.
#define SECRET_STA_SSID     ""
#define SECRET_STA_PASSWORD ""

#endif /* SECRETS_H_ */
