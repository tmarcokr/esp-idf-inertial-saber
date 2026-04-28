# Especificación Técnica: InertialLight Engine

## 1. Visión General del Sistema
El **InertialLight Engine** es el subsistema visual complementario al *InertialSwing*. Su objetivo es traducir las fuerzas cinéticas y la acumulación de energía del sable en respuestas lumínicas orgánicas, utilizando el espacio de color HSB (Hue, Saturation, Brightness).

El comportamiento visual ya no es estático ni depende de animaciones pre-grabadas; es una respuesta en tiempo real a las métricas del motor de audio:
* `EnergiaTotal` (Fuerza G instantánea).
* `TanqueOverload` (Acumulador de inercia de 0.0 a 1.0).
* `BladeAngle` (Inclinación física del sable).

---

## 2. Definición del Espacio de Color (HSB)
Para facilitar las transiciones de temperatura y explosiones complementarias, toda la matemática visual se calcula en HSB.
* **H (Hue / Tono):** 0-360 grados. Define el color base del sable (Ej: 240 = Azul, 0 = Rojo, 120 = Verde).
* **S (Saturation / Saturación):** 0.0 a 1.0. Determina la pureza. (1.0 = Color puro, 0.0 = Blanco puro).
* **B (Brightness / Brillo):** 0.0 a 1.0. Intensidad de la luz.

---

## 3. Estados Visuales y Matemáticas

### 3.1. Estado 1: "Respiración Viva" (Calma)
**Condición:** `EnergiaTotal < 0.5 G` y `TanqueOverload == 0.0`
El sable no está apagado, sino conteniendo energía. Emite un pulso suave cuyo ritmo depende de la gravedad (Gravity Tonal Modulator).

**Fórmulas:**
1.  **Frecuencia base de respiración:** `Freq = 1.0 Hz`
2.  **Modulador de Gravedad:** Si el sable apunta arriba (+90°), respira más rápido (el calor sube). Si apunta abajo (-90°), respira más lento.
    `FrecuenciaActual = Freq + (sin(BladeAngle) * 0.5)`
3.  **Cálculo de Brillo (Onda Senoidal):**
    `Pulso = (sin(Tiempo * FrecuenciaActual * 2 * PI) + 1.0) / 2.0`
    `BrilloFinal = 0.85 + (Pulso * 0.15)` *(Oscila suavemente entre 85% y 100%)*
4.  **Color:** `H = BaseHue`, `S = 1.0` (Color puro).

### 3.2. Estado 2: "Excitación Térmica" (Movimiento y Carga)
**Condición:** `EnergiaTotal >= 0.5 G` o `TanqueOverload > 0.0`
El movimiento empuja el brillo al máximo, mientras que el nivel del tanque de inercia "calienta" el plasma, empujando el color hacia el blanco (pérdida de saturación térmica).

**Fórmulas:**
1.  **Brillo Recreativo:** El brillo se mantiene al 100%, pero se le inyecta un *flicker* (parpadeo) caótico basado en la fuerza G instantánea.
    `Ruido = Random(-1.0, 1.0) * clamp(EnergiaTotal / 4.0, 0.0, 1.0)`
    `BrilloFinal = clamp(1.0 - abs(Ruido * 0.2), 0.8, 1.0)` *(Flicker entre 80% y 100%)*
2.  **Calentamiento del Plasma (Saturación):** A medida que el tanque sube a 1.0 (100%), la saturación cae hasta un 20%, volviendo el núcleo del sable casi blanco.
    `SaturacionFinal = 1.0 - (TanqueOverload * 0.8)`

### 3.3. Estado 3: "Rotura de Plasma" (Inertial Burst)
**Condición:** Disparado instantáneamente cuando `TanqueOverload == 1.0`
La liberación violenta de energía produce una sobrecarga visual de un frame/ciclo corto (ej. 100ms - 200ms) usando el color complementario de la teoría del color para máximo contraste.

**Fórmulas (Gatillo único):**
1.  **Color Complementario (Desfase de 180°):**
    `BurstHue = (BaseHue + 180) % 360`
2.  **Restauración de Saturación:** El destello debe ser de color puro para contrastar con el blanco anterior.
    `SaturacionFinal = 1.0`
3.  **Super-Brillo:** Si el hardware lo permite (ej. apagando canales temporalmente para dar más voltaje a otros), forzar máxima luminosidad.
    `BrilloFinal = 1.0`

*Nota Visual:* Después del tiempo de *BurstCooldown* (ej. 300ms definido en el motor de audio), la luz hace un "fade" (transición suave de 150ms) de vuelta al color base y al estado de Calma.

---

## 4. Resumen de Tuning Lumínico (Parámetros)

| Parámetro | Valor Sugerido | Descripción |
| :--- | :--- | :--- |
| **IdlePulseDepth** | 0.15 (15%) | Profundidad del oscilador en estado de Calma. |
| **IdleBaseFreq** | 1.0 Hz | Ciclos de respiración por segundo en horizontal. |
| **MaxThermalBleed** | 0.80 (80%) | Cuánta saturación se pierde al 100% de Overload. |
| **BurstDuration** | 150ms | Duración visual del destello de Rotura de Plasma. |
| **FlickerIntensity** | 0.20 (20%) | Caos en el brillo introducido por las fuerzas G. |

## 5. Lógica de Ejecución (Pseudocódigo)

```text
// Loop ejecutado a la velocidad de actualización del LED (ej. 100Hz)

Leer métricas de InertialSwing (EnergiaTotal, TanqueOverload, BladeAngle)

SI (Evento_Rotura_De_Plasma_Detectado):
    Aplicar Destello_Complementario (Hue + 180, S=1.0, B=1.0)
    Iniciar Temporizador_Burst

SI NO (Temporizador_Burst_Activo):
    SI (TanqueOverload > 0.0 O EnergiaTotal > 0.5):
        // ESTADO 2: EXCITACIÓN
        Hue = BaseHue
        Saturation = 1.0 - (TanqueOverload * MaxThermalBleed)
        Brightness = Calcular_Flicker_G(EnergiaTotal)
    SINO:
        // ESTADO 1: RESPIRACIÓN
        Hue = BaseHue
        Saturation = 1.0
        Brightness = Calcular_Pulso_Gravedad(BladeAngle, Tiempo)
        
Enviar a Controlador LED (Convertir HSB a RGB)
```