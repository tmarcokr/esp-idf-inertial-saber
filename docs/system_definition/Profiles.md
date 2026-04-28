# Especificación Funcional: Sistema de Perfiles (InertialProfile)

## 1. Visión General del Sistema
El `InertialProfile` es el componente encargado de definir la identidad estética, sonora y reactiva del sable. A diferencia de sistemas basados en archivos de configuración externos, este modelo utiliza **Clases Compiladas** para garantizar la máxima velocidad de ejecución en el ESP32. 

Cada perfil encapsula la lógica de los motores inerciales (`InertialDefinition`) y la colección de efectos discretos (`InertialEffect`) que el sable puede ejecutar, actuando como el "Contenedor de Personalidad" del sistema.

## 2. Organización de Recursos en la SD
Aunque la inteligencia y las reglas de disparo están compiladas, los recursos pesados (archivos de audio `.wav`) se almacenan en la tarjeta SD siguiendo una estructura jerárquica que el perfil referencia dinámicamente.

```text
/SD/
└── profiles/
    └── sith_lord/
        ├── impact/      <-- Ficheros para InertialEffect de choque (clash)
        ├── blaster/     <-- Ficheros para InertialEffect de disparo (blast)
        ├── force/       <-- Ficheros para efectos de Fuerza
        └── motion/      <-- Recursos para los motores core (Inertial Engines)
            ├── hum.wav
            ├── swingh/  <-- Variaciones agudas (swingH)
            └── swingl/  <-- Variaciones graves (swingL)
```

## 3. Arquitectura de Clases: El Contrato de Efecto
El sistema se basa en una interfaz de efectos que permite al `SaberAction System` evaluar y ejecutar acciones de forma modular y asíncrona.

### Interfaz `InertialEffect`
Cada efecto (choque, disparo, etc.) es una clase que implementa dos métodos fundamentales:

*   **Test(SaberDataPacket):** Método de evaluación que recibe el streaming de sensores a alta frecuencia (~800Hz) y devuelve un booleano si las condiciones (G-Force, rotación o entrada de botones) se cumplen para disparar el efecto.
*   **Run():** Método de ejecución que coordina la salida de audio (WAV) y la respuesta lumínica (LED) asociada al efecto en el momento del disparo.

```cpp
// Estructura lógica del contrato de efecto
class InertialEffect {
public:
    uint8_t Priority;         // Nivel de prioridad (0-3: Background a System)
    virtual bool Test(const SaberDataPacket& packet) = 0; // ¿Cuándo ocurre?
    virtual void Run() = 0;   // ¿Qué hace? (Audio + Luz)
};
```

## 4. Definición del Motor Inercial (InertialDefinition)
Esta clase actúa como el plano de configuración técnica para los motores core de `InertialSwing` e `InertialLight`. Define los parámetros físicos y las rutas de recursos que los algoritmos utilizarán para ese perfil específico.

### Propiedades de `InertialDefinition`
*   **Parámetros Inerciales:** Límites de G-Force, sensibilidad del acumulador (`OverloadSensitivity`) y tasas de vaciado (`OverloadDrainRate`).
*   **Parámetros Lumínicos:** Color base (HSB), frecuencia de la "respiración" (`IdleBaseFreq`) y profundidad del pulso (`IdlePulseDepth`).
*   **Mapeo de Audio Core:** Listas compiladas con las rutas a los directorios o ficheros de `hum`, `swingH` y `swingL` en la tarjeta SD.

## 5. Gestión de Carga y el Bus de Acciones
Los perfiles no se escanean en tiempo de ejecución; se instancian y se cargan mediante el método `.load()`, que inyecta la lógica y los recursos en el `SaberAction Bus`.

### Lógica de `InertialProfile.load()`
Cuando el usuario selecciona un perfil (ej. `SithLord.load()`), se ejecutan las siguientes acciones de inyección:

1.  **Configuración Core:** Entrega la `InertialDefinition` a los motores de audio y luz para establecer los límites físicos y comenzar la carga de los sonidos base (Hum/Swings).
2.  **Registro de Acciones:** El perfil entrega su colección de objetos `InertialEffect` al `SaberAction Bus`.
3.  **Evaluación Activa:** El Bus comienza a pasar el `SaberDataPacket` de forma inmediata a través del método `Test()` de cada efecto registrado para detectar disparos.

## 6. Resumen Funcional de Capas

| Capa | Componente | Naturaleza | Función Principal |
| :--- | :--- | :--- | :--- |
| **Recursos** | Audio WAV | SD Card | Datos binarios de sonido organizados por carpetas. |
| **Configuración** | `InertialDefinition` | Clase Compilada | Define límites físicos, colores HSB y rutas de audio core. |
| **Lógica** | `InertialEffect` | Clase Compilada | Contiene el Test (condición) y el Run (renderizado). |
| **Identidad** | `InertialProfile` | Clase Compilada | Agrupa la definición inercial y la lista de efectos. |
| **Control** | `SaberAction Bus` | Motor Core | Evalúa el streaming de datos contra las reglas del perfil. |
