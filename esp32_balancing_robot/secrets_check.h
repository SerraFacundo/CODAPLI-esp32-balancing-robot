/*
 * secrets_check.h
 *
 * Incluye secrets.h (claves que no se suben al repositorio) y valida en tiempo
 * de compilación que exista y que no tenga valores inválidos o de ejemplo.
 * Está en un .h y no en el .ino para que el preprocesador de Arduino no genere
 * prototipos de las funciones constexpr.
 */

#ifndef SECRETS_CHECK_H_
#define SECRETS_CHECK_H_

#include <stddef.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Falta esp32_balancing_robot/secrets.h: copiar esp32_balancing_robot/secrets.example.h como secrets.h y completar las claves."
#endif

// Helpers constexpr (compatibles con C++11, usados también en el .ino)
constexpr size_t cstrLen(const char *s) { return *s ? 1 + cstrLen(s + 1) : 0; }
constexpr bool cstrEq(const char *a, const char *b) {
  return (*a == *b) && (*a == '\0' || cstrEq(a + 1, b + 1));
}

static_assert(cstrLen(SECRET_AP_PASSWORD) >= 8 && cstrLen(SECRET_AP_PASSWORD) <= 63,
              "SECRET_AP_PASSWORD debe tener entre 8 y 63 caracteres (WPA2)");
static_assert(cstrLen(SECRET_WS_TOKEN) >= 8,
              "SECRET_WS_TOKEN debe tener al menos 8 caracteres (recomendado 16+)");
static_assert(!cstrEq(SECRET_WS_TOKEN, "cambiar-este-token") &&
              !cstrEq(SECRET_AP_PASSWORD, "cambiar-esta-clave"),
              "secrets.h todavía tiene los valores de ejemplo: cambiarlos");

#endif /* SECRETS_CHECK_H_ */
