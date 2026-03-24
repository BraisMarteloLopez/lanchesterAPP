# Plan de Interfaz Grafica — Lanchester-CIO v2

Diseño de la nueva interfaz grafica de la aplicacion.
Este plan es **independiente** de la refactorizacion del backend — ver `PLAN_REFACTORIZACION.md` para la arquitectura OOP.

**Estado: PENDIENTE DE DEFINIR**

---

## Pre-requisito

La implementacion de este plan requiere que las fases 0-4 de `PLAN_REFACTORIZACION.md` esten completadas.
En particular:

- **`SimulationService`** debe estar operativo como API unica de simulacion.
- **`ScenarioConfig`** debe ser el contrato de datos entre UI y motor.
- **`src/ui/`** debe estar desacoplado del dominio (la UI actual es reemplazable).

---

## Concepto general

Interfaz tipo **wizard** (asistente paso a paso) que guia al usuario por la configuracion del escenario. La ultima pantalla es una **presentacion 2D** del resultado del combate.

### Flujo previsto

```
[Pantalla 1]  ──>  [Pantalla 2]  ──>  [Pantalla 3]  ──>  [Pantalla 4]  ──>  [Pantalla 5]
  Terreno &         Fuerza             Fuerza           Parametros de        Resultado
  Distancia          Azul               Roja             Simulacion           2D
```

### Infraestructura base

El wizard se construira sobre las abstracciones definidas en la refactorizacion:

```cpp
// Interfaz de pantalla (definida en PLAN_REFACTORIZACION.md, fase 4)
class IScreen {
public:
    virtual ~IScreen() = default;
    virtual void render() = 0;
    virtual bool isComplete() const = 0;
    virtual std::string title() const = 0;
    virtual void onEnter() {}
    virtual void onExit() {}
};

// Gestor de navegacion
class WizardManager {
public:
    void addScreen(std::unique_ptr<IScreen> screen);
    void render();   // pantalla actual + barra de navegacion
    ScenarioConfig& config();   // estado compartido que el wizard construye
};
```

---

## Pantallas (por definir en detalle)

### Pantalla 1: Terreno y distancia

Configuracion del escenario fisico: tipo de terreno, distancia de enfrentamiento, movilidad de cada bando.

**Datos que produce:** `config.terrain`, `config.distance_m`, movilidades.

### Pantalla 2: Fuerza azul

Seleccion de vehiculos del catalogo NATO, cantidades, estado tactico y parametros avanzados.

**Datos que produce:** `config.blue`

### Pantalla 3: Fuerza roja

Idem pantalla 2 con catalogo OPFOR.

**Datos que produce:** `config.red`

### Pantalla 4: Parametros de simulacion

Modo (determinista/MC), agregacion, t_max, replicas/seed. Resumen pre-ejecucion y boton de lanzamiento.

**Datos que produce:** modo de simulacion, parametros MC.

### Pantalla 5: Resultado 2D

Visualizacion de planta (2D top-down) del campo de batalla con representacion de unidades, animacion temporal del combate, y panel de metricas.

**Por diseñar:** layout, iconografia, controles de reproduccion, nivel de detalle visual.

---

## Pendiente

- [ ] Definir mockups/wireframes de cada pantalla
- [ ] Decidir nivel de detalle de la presentacion 2D (iconos simples vs sprites, animacion continua vs por pasos)
- [ ] Definir la interaccion: ratones, teclado, tooltips
- [ ] Decidir si la pantalla 2D usa ImGui canvas, OpenGL directo, o una libreria 2D adicional
- [ ] Definir los controles de reproduccion de la animacion

---

## Relacion con otros documentos

| Documento | Contenido |
|---|---|
| **PLAN_REFACTORIZACION.md** | Arquitectura OOP, dominio, servicios, build, tests |
| **PLAN_INTERFAZ.md** (este) | Diseño de la nueva interfaz grafica. Pendiente de definir |
| **DEUDA_TECNICA.md** | Registro historico de deuda tecnica |

---

*Documento de planificacion interna. Lanchester-CIO v2.*
*Creado: 2026-03-24*
