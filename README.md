# Modelo Lanchester-CIO

Herramienta de investigacion interna del CIO/ET. Calcula el resultado de enfrentamientos entre fuerzas terrestres usando ecuaciones de Lanchester: vencedor, bajas, duracion, municion consumida.

## Compilacion

```bash
make
```

Requisitos: compilador C++17 (g++ o clang++). Sin dependencias externas — `nlohmann/json` esta incluida localmente.

## Uso

### Escenario unico

```bash
./lanchester escenario.json
./lanchester escenario.json --output resultado.json
```

### Batch (directorio de escenarios)

```bash
./lanchester --batch escenarios/ --output resultados.csv
```

Procesa todos los `.json` del directorio. Salida CSV con separador `;`.

### Barrido parametrico (sweep)

```bash
# Barrer distancia de enfrentamiento
./lanchester --scenario base.json --sweep engagement_distance_m 500 5000 100

# Barrer numero de vehiculos
./lanchester --scenario base.json --sweep "combat_sequence[0].blue.composition[0].count" 1 20 1

# Barrer fraccion de empenamiento
./lanchester --scenario base.json --sweep "combat_sequence[0].blue.engagement_fraction" 0.1 1.0 0.1
```

Salida CSV. Soporta paths con notacion punto y corchetes.

### Modo de agregacion

```bash
./lanchester escenario.json --aggregation post
```

- `pre` (por defecto): media ponderada de parametros, una tasa unica.
- `post`: tasa individual por tipo de vehiculo, luego media ponderada de tasas. Mas realista para fuerzas heterogeneas.

## Formato de entrada

```json
{
  "scenario_id": "ALFA-001",
  "terrain": "MEDIO",
  "engagement_distance_m": 3000,
  "solver": { "h": 0.001666, "t_max_minutes": 30.0 },
  "combat_sequence": [
    {
      "combat_id": 1,
      "blue": {
        "tactical_state": "En posicion de tiro",
        "mobility": "ALTA",
        "aft_received": 5,
        "aft_casualties_pct": 0.025,
        "engagement_fraction": 0.666,
        "rate_factor": 1.0,
        "count_factor": 1.0,
        "composition": [
          { "vehicle": "TOA_SPIKE_I", "count": 6 }
        ]
      },
      "red": {
        "tactical_state": "Ataque a posicion defensiva",
        "mobility": "ALTA",
        "aft_received": 0,
        "aft_casualties_pct": 0.0,
        "engagement_fraction": 0.666,
        "rate_factor": 1.0,
        "count_factor": 1.0,
        "composition": [
          { "vehicle": "T-80U", "count": 10 }
        ]
      },
      "reinforcements_blue": [],
      "reinforcements_red": [],
      "displacement_distance_m": 0
    }
  ]
}
```

| Campo | Valores |
|---|---|
| `terrain` | `FACIL`, `MEDIO`, `DIFICIL` |
| `tactical_state` | `Ataque a posicion defensiva`, `Busqueda del contacto`, `En posicion de tiro`, `Defensiva condiciones minimas`, `Defensiva organizacion ligera`, `Defensiva organizacion media`, `Retardo`, `Retrocede` |
| `mobility` | `MUY_ALTA`, `ALTA`, `MEDIA`, `BAJA` |
| `rate_factor` | Multiplicador de tasa de destruccion [0.0-2.0], defecto 1.0 |
| `count_factor` | Multiplicador de vehiculos efectivos [0.0-2.0], defecto 1.0 |
| `aft_casualties_pct` | Porcentaje de bajas por AFT pre-contacto [0.0-1.0] |
| `engagement_fraction` | Fraccion de la fuerza que entra en combate [0.0-1.0] |

## Formato de salida

```json
{
  "scenario_id": "ALFA-001",
  "combats": [
    {
      "combat_id": 1,
      "outcome": "RED_WINS",
      "duration_contact_minutes": 1.12,
      "duration_total_minutes": 1.12,
      "blue_initial": 3.9,
      "red_initial": 6.66,
      "blue_survivors": 0.0,
      "red_survivors": 6.38,
      "blue_casualties": 3.9,
      "red_casualties": 0.28,
      "blue_ammo_consumed": 4.33,
      "red_ammo_consumed": 21.62,
      "blue_cc_ammo_consumed": 1.08,
      "red_cc_ammo_consumed": 0.0,
      "static_advantage": 0.1278
    }
  ]
}
```

| Outcome | Significado |
|---|---|
| `BLUE_WINS` | Fuerza roja eliminada, azul tiene supervivientes |
| `RED_WINS` | Fuerza azul eliminada, rojo tiene supervivientes |
| `DRAW` | Ambas fuerzas eliminadas simultaneamente |
| `INDETERMINATE` | Tiempo maximo alcanzado sin vencedor |

`static_advantage`: ratio de Lanchester en t=0. Valores > 1 favorecen a azul.

## Catalogos de vehiculos

- `vehicle_db.json` — vehiculos propios (bando azul): TOA_SPIKE_I, VEC_25, LEOPARDO_2E, PIZARRO
- `vehicle_db_en.json` — vehiculos enemigos (bando rojo): T-80U, BMP-3, T-72B3, BTR-82A

Busqueda cruzada: si un vehiculo no se encuentra en el catalogo de su bando, se busca en el otro.

## Encadenamiento de combates

Se soportan cadenas de hasta 3 combates sucesivos. Los supervivientes del combate N pasan al N+1, sumando refuerzos declarados. La municion C/C de los refuerzos es completa. El tiempo total incluye desplazamiento entre posiciones.

## Validacion

```bash
bash tests/run_validation.sh
```

46 tests cubriendo: simetria, fuerza abrumadora, sin C/C, fuera de alcance, bajas AFT, multiplicadores defensivos, fuerzas mixtas, factores de ajuste. Incluye comparativa pre-tasa vs post-tasa.

## Estructura

```
├── main.cpp                  # Ejecutable (~950 lineas)
├── vehicle_db.json           # Catalogo vehiculos azul
├── vehicle_db_en.json        # Catalogo vehiculos rojo
├── include/nlohmann/json.hpp # Dependencia JSON header-only
├── Makefile
├── ejemplos/
│   ├── toa_vs_t80u.json      # Escenario simple
│   ├── compania_mixta.json   # Cadena de 2 combates
│   ├── sweep_distancia.sh    # Sensibilidad: distancia
│   ├── sweep_count.sh        # Sensibilidad: numero vehiculos
│   └── sweep_engagement.sh   # Sensibilidad: fraccion empenamiento
├── tests/
│   ├── run_validation.sh     # Script de validacion
│   └── test_*.json           # 8 escenarios de prueba
├── Plan_Port_Lanchester_CPP.md
├── DEUDA_TECNICA.md
└── VALIDACION_SESION3.md
```

---

*CIO / ET — Herramienta de investigacion interna*
