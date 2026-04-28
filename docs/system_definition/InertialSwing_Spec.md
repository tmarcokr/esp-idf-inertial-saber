# Especificación Técnica: InertialSwing Engine

## 1. Visión General del Sistema
El **InertialSwing Engine** es un modelo de audio de alto rendimiento diseñado para el ESP32. Este motor se basa en la **Energía Cinética Lineal** y la **Inercia Física**, permitiendo que el sable reaccione a cualquier movimiento (pasos, estocadas, giros) de forma orgánica y cinematográfica.

El motor se divide en cuatro subsistemas:
1. **Inertial Crossfade**: Controla el volumen y la mezcla base según la fuerza del movimiento.
2. **Inertial Overload**: Un acumulador que dispara ráfagas de sonido agresivo tras un esfuerzo físico real.
3. **Gravity Tonal Modulator**: Ajusta el "color" del sonido según la orientación del sable.
4. **Zero-Volume SD Swapper**: Gestiona la carga de archivos en la SD sin generar ruidos ni latencia.

---

## 2. Cálculo de Energía y Mezcla (Inertial Crossfade)
El sistema mide la energía total del movimiento sumando los vectores de aceleración 3D (restando la gravedad).

**Fórmula de Energía:**
`EnergiaTotal = sqrt(mss_x^2 + mss_y^2 + mss_z^2)`

### 2.1. Umbrales de Intensidad
La `EnergiaTotal` define el volumen maestro y el balance entre los archivos `swingL` (Grave) y `swingH` (Agudo).

| Estado | Energía (G) | Acción del Mezclador |
| :--- | :--- | :--- |
| **Calma** | 0.0 - 0.5 G | Solo Hum audible. Volumen Swing = 0%. |
| **Zona L** | 0.5 - 2.0 G | `swingL` domina. Volumen maestro en ascenso. |
| **Transición** | 2.0 - 4.0 G | Crossfade activo: `swingL` baja y `swingH` sube. |
| **Zona H** | > 4.0 G | `swingH` domina al volumen máximo. |

---

## 3. Modulación por Gravedad (Gravity Tonal Modulator)
El ángulo de la hoja (`blade_angle`) actúa como un ecualizador que "colorea" el sonido, dándole más peso a una u otra textura según la dirección.

* **Sable Arriba (+90°):** Sonido etéreo. Inyecta un +20% de peso a `swingH`.
* **Sable Horizontal (0°):** Tono neutral definido solo por la Intensidad.
* **Sable Abajo (-90°):** Sonido pesado/rugiente. Inyecta un +20% de peso a `swingL`.

**Cálculo del Mix Final:**
1. `ModuladorGravedad = sin(blade_angle)`
2. `MixBase = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`
3. `MixFinal = clamp(MixBase + (ModuladorGravedad * 0.2), 0.0, 1.0)`

---

## 4. Gestión de Archivos (Zero-Volume SD Swapper)
Para evitar saturar el bus I2C/SD del ESP32, solo se carga un par de archivos (`swingL/H`) a la vez. El cambio ocurre en el "Momento Invisible".

### 4.1. Diagrama de Flujo (TextArt)

```text
[ CALMA ] ───────────( Energía >= 0.5G )───────────┐
  Volumen: 0%                                      │
  SD: Listo                                        ▼
      ▲                                     [ MOVIMIENTO ]
      │                                      Volumen: > 0%
      │                                      Mezcla activa
      │                                            │
  ( MOMENTO INVISIBLE )                            │
  1. Volumen real == 0                             ▼
  2. Cerrar L1/H1                       [ DESVANECIMIENTO ]
  3. Abrir L2/H2 de SD                   Volumen cae a 0%
  4. Estado -> CALMA                     Energía < 0.5G
      ▲                                            │
      └──────────( Volumen == 0.0 )────────────────┘
```

## 5. Sobrecarga de Energía (Inertial Overload)
Representa un tanque de inercia virtual que requiere un esfuerzo físico real para cargarse y que se libera de forma explosiva cuando el aire "se rompe" por la velocidad de la hoja.

### 5.1. Lógica del Acumulador
Para evitar el efecto ametralladora (disparos repetitivos), el sistema exige una acumulación de fuerza sostenida antes de generar un estallido sonoro.

1.  **Carga Activa**: Solo si la `EnergiaTotal` supera el umbral de 3.5 G, el tanque comienza a llenarse proporcionalmente al exceso de fuerza aplicada.
2.  **Fuga de Inercia**: Si el movimiento cae por debajo de 3.5 G, el tanque se vacía rápidamente. Esto simula la pérdida de inercia y evita que movimientos lentos acumulados disparen el efecto por error.
3.  **Inertial Burst**: Al alcanzar el 100%, el motor envía una orden al mezclador polifónico para reproducir un archivo `swng` o `slsh` y resetea el tanque a cero inmediatamente.
4. **Cooldown**: Tras el Burst, el tanque permanece bloqueado durante BurstCooldown ms antes de aceptar nueva carga.

### 5.2. Representación Visual del Flujo

```text
 ENERGÍA (Fuerza G)        ESTADO DEL TANQUE (0 - 100%)        ACCIÓN DE AUDIO
 ------------------        ----------------------------        ---------------
 G < 3.5G (Baja)           [          vacío           ]        Inertial Crossfade
                                 (Fuga activa)

 G = 4.5G (Media)          [|||||||                   ]        Cargando...
                             (Llenado proporcional)

 G = 7.0G (Alta)           [||||||||||||||||||||||||||] 100%   💥 INERTIAL BURST
                                 (Reset a 0)                   (Volumen Máximo)
```

## 6. Integración del Mezclador (Fórmulas Finales)

Para la implementación en el ESP32, el volumen de cada canal se calcula en cada frame (800Hz) siguiendo este orden de precedencia:

### 6.1. Cálculo del Volumen Maestro de Swing
Determina la presencia global del sonido de movimiento basándose en la inercia total.
`Volumen_Maestro = clamp((EnergiaTotal - 0.5) / (4.0 - 0.5), 0.0, 1.0)`

### 6.2. Balance Tonal (Inertial Crossfade + Gravity)
Calcula la proporción entre los dos archivos cargados en memoria.
1. `MixBase = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`
2. `ModGravedad = sin(blade_angle) * 0.2`
3. `MixFinal = clamp(MixBase + ModGravedad, 0.0, 1.0)`

### 6.3. Salida por Canal
`Volumen_SwingL = Volumen_Maestro * (1.0 - MixFinal)`
`Volumen_SwingH = Volumen_Maestro * MixFinal`

---

## 7. Notas de Ajuste (Tuning)

| Parámetro | Valor Sugerido | Descripción |
| :--- | :--- | :--- |
| **OverloadSensitivity** | 5.0 | A mayor valor, menos esfuerzo físico para disparar un Burst. |
| **OverloadDrainRate** | 10.0 | Velocidad de vaciado. Debe ser > Llenado para evitar acumulación errónea. |
| **BurstCooldown** | 300ms | Tiempo mínimo de rearmado del tanque tras un disparo exitoso. |
| **GravityInfluence** | 0.2 (20%) | Cuánto afecta la inclinación al balance L/H. |