/*
 * secrets.example.h
 *
 * Copiar este archivo como secrets.h (mismo directorio) y completar los valores.
 * secrets.h está en .gitignore: las claves reales no se suben al repositorio.
 *
 * (Todavía no se incluye desde el firmware: se usa en la tarea de seguridad,
 *  objetivo 7 — token del WebSocket y nueva contraseña del AP.)
 */

#ifndef SECRETS_H_
#define SECRETS_H_

// Contraseña del AP WiFi del robot (WPA2, mínimo 8 caracteres)
#define SECRET_AP_PASSWORD "cambiar-esta-clave"

// Token que el cliente debe enviar (T:<token>) antes de que se acepten comandos
#define SECRET_WS_TOKEN    "cambiar-este-token"

// Red de la laptop para modo AP+STA (plataforma IIoT)
#define SECRET_STA_SSID     "nombre-de-la-red"
#define SECRET_STA_PASSWORD "clave-de-la-red"

#endif /* SECRETS_H_ */
