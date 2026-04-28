# Especificación Técnica: SaberAction System (The Action Bus)

## 1. Visión General del Sistema
El **SaberAction System** es un despachador de eventos asíncrono y modular diseñado para el procesamiento de datos en tiempo real. Su función principal es actuar como un **Bus de Datos** intermedio entre los productores de información (Sensores IMU y Periféricos de Entrada) y los consumidores de eventos (Motores de Audio y Luz).

El sistema permite la suscripción dinámica de "Acciones" a un flujo constante de datos (**Data Stream**), donde cada acción es responsable de evaluar su propia condición de disparo basándose en el estado global del sistema.

---

## 2. Estructura del Stream de Datos (`SaberDataPacket`)
El bus distribuye un paquete de datos unificado en cada ciclo de actualización. Este paquete contiene el estado instantáneo de todos los descriptores físicos y de interfaz del sable.

### 2.1. Descriptores de Movimiento (Kinetic Data)
| Atributo | Tipo | Descripción |
| :--- | :--- | :--- |
| `KineticEnergy` | `float` | Fuerza G total calculada por el motor de inercia. |
| `AxisRotation` | `float[3]` | Velocidad angular en los ejes X, Y, Z (deg/s). |
| `OrientationVector` | `float` | Ángulo de inclinación de la hoja respecto a la gravedad. |

### 2.2. Descriptores de Interfaz (Input Data)
| Atributo | Tipo | Descripción |
| :--- | :--- | :--- |
| `InputID` | `uint8_t` | Identificador del periférico de entrada (Botón 0...N). |
| `InputState` | `enum` | Estado lógico: `IDLE`, `ACTIVE`, `HOLD`, `RELEASED`. |
| `TriggerDuration` | `uint32_t` | Tiempo transcurrido (ms) en el estado actual. |
| `InputCounter` | `uint8_t` | Registro de activaciones rápidas (multi-clic). |

---

## 3. Tipología de Acciones (The InertialEffect Interface)
Todas las acciones deben implementar la interfaz `InertialEffect`, que les permite ser evaluadas por el bus. Se definen tres categorías lógicas según su comportamiento:

### 3.1. Moduladores de Flujo (Continuous Actions)
Acciones que operan de forma persistente (Prioridad 0). No esperan un evento de disparo, sino que transforman datos del stream en parámetros de salida (ej. `InertialSwing` mapeando fuerza G a volumen).

### 3.2. Disparadores de Evento (Discrete Actions)
Acciones con una firma de activación única. Implementan el método `Test()` para evaluar si se cumple una coincidencia exacta de descriptores (ej. choque o disparo de blaster).

### 3.3. Detectores de Patrones (Sequential Actions)
Acciones que mantienen un buffer interno de estados anteriores. Se activan tras detectar una secuencia cronológica de cambios en los descriptores dentro de una ventana de tiempo definida.

---

## 4. Jerarquía y Gestión de Conflictos (Priority System)
Para gestionar la interacción entre múltiples acciones que operan simultáneamente sobre los mismos recursos (canales de audio o LEDs), el sistema implementa una tabla de prioridades dentro de cada `InertialEffect`.

| Nivel | Categoría | Comportamiento |
| :--- | :--- | :--- |
| **0** | **Background** | Acciones persistentes (Hum/Swing). Pueden ser atenuadas (ducking). |
| **1** | **Standard** | Eventos de prioridad normal (Blaster). Se mezclan de forma aditiva. |
| **2** | **Override** | Eventos de alta prioridad (Clash). Provocan atenuación forzada en niveles inferiores. |
| **3** | **System** | Eventos críticos de hardware (Power On/Off). Control total sobre el bus. |

---

## 5. Gestión Dinámica de Perfiles (InertialProfile)
El sistema permite el intercambio de la lista de acciones activas en tiempo de ejecución.

1. **Carga de Perfil:** El `InertialProfile` inyecta su `InertialDefinition` a los motores core y registra sus `InertialEffect` en el bus.
2. **Limpieza:** El bus asegura la correcta liberación de memoria de las acciones del perfil anterior.
3. **Ejecución:** El loop principal itera sobre la lista activa, pasando la referencia del `SaberDataPacket` al método `Test()` de cada efecto.

---

## 6. Lógica de Ejecución (Pseudocódigo Abstracto)

```cpp
// Contrato base para cualquier acción del sistema
class InertialEffect {
public:
    uint8_t Priority;
    virtual bool Test(const SaberDataPacket& packet) = 0; // Evaluación
    virtual void Run() = 0;                               // Renderizado
};

// Loop principal de procesamiento (Update Rate: ~1ms)
while (system_active) {
    SaberDataPacket currentPacket = HardwareScanner::generatePacket();
    
    for (auto& effect : activeProfile.effects) {
        if (effect->Test(currentPacket)) {
            effect->Run();
        }
    }
    
    OutputRenderer::flush();
}
```