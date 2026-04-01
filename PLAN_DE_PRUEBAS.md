# Plan de Pruebas — Lanchester-CIO (Windows GUI)

Guia paso a paso para probar el simulador de combate Lanchester en Windows.

> **Nota:** Ademas de estas pruebas manuales, el proyecto tiene **28 tests automatizados** (Catch2 v3) que se ejecutan en Linux con `ctest --test-dir build --output-on-failure`. Los tests automatizados cubren: carga de parametros, catalogos, modelo matematico (SquareLawModel vs baseline), servicio de simulacion y validacion de configuraciones. Ver `src/tests/` para el codigo fuente.

---

## PARTE 1: Preparar el entorno

### Paso 1.1 — Obtener la aplicacion

Copia la carpeta `release/` completa a tu PC con Windows. Debe contener:

- `lanchester_gui.exe` — Aplicacion principal
- `SDL2.dll` — Libreria grafica (necesaria)
- `model_params.json` — Parametros del modelo
- `vehicle_db.json` — Catalogo vehiculos azul
- `vehicle_db_en.json` — Catalogo vehiculos rojo

**Importante:** Todos los ficheros deben estar en la misma carpeta.

### Paso 1.2 — Ejecutar

Haz doble clic en `lanchester_gui.exe`. Se abre la ventana del simulador.

Si Windows muestra un aviso de seguridad ("Windows protegió su PC"), pulsa **Más información** y luego **Ejecutar de todas formas**.

---

## PARTE 2: Verificacion rapida

### Paso 2.1 — Comprobar que carga los catalogos

Al abrir la aplicacion, los desplegables de vehiculos deben mostrar:
- Bando azul: LEOPARDO_2E, PIZARRO, TOA_SPIKE_I, VEC_25
- Bando rojo: BMP-3, BTR-82A, T-72B3, T-80U

Si aparece un mensaje de error sobre catalogos, verifica que los ficheros JSON estan junto al .exe.

### Paso 2.2 — Ejecutar un escenario basico

1. Deja la configuracion por defecto (10 LEOPARDO_2E vs 10 T-80U)
2. Pulsa **EJECUTAR SIMULACION**
3. En el panel derecho debe aparecer un resultado (BLUE_WINS, RED_WINS, DRAW o INDETERMINATE)

---

## PARTE 3: Pruebas del modo determinista

### Test 3.1 — Combate simetrico

- Bando azul: 10 LEOPARDO_2E
- Bando rojo: 10 LEOPARDO_2E (nota: requiere que ambos bandos usen el mismo vehiculo; si la GUI no permite seleccionar vehiculos del catalogo contrario, usar 10 LEOPARDO_2E vs 10 LEOPARDO_2E o configurar un escenario con parametros identicos)
- Terreno: MEDIO, Distancia: 2000m
- **Esperado**: DRAW, ventaja estatica cercana a 1.0

### Test 3.2 — Fuerza abrumadora

- Bando azul: 20 LEOPARDO_2E
- Bando rojo: 2 T-80U
- **Esperado**: BLUE_WINS, supervivientes azul > 18

### Test 3.3 — Fuera de alcance

- Distancia: 9000m
- Cualquier composicion
- **Esperado**: INDETERMINATE, 0 bajas (nadie alcanza al otro)

### Test 3.4 — Fuerzas mixtas

- Bando azul: 6 LEOPARDO_2E + 6 PIZARRO
- Bando rojo: 8 T-80U + 8 BMP-3
- **Esperado**: Resultado con municion C/C consumida > 0

### Test 3.5 — Bajas AFT pre-contacto

- Bando azul: 10 LEOPARDO_2E, Bajas AFT = 50%
- Bando rojo: 10 T-80U
- **Esperado**: Azul inicia con ~5 efectivos

### Test 3.6 — Defensa preparada

- Bando azul: 5 LEOPARDO_2E, Estado: "Defensiva organizacion media"
- Bando rojo: 10 T-80U, Estado: "Ataque a posicion defensiva"
- **Esperado**: Azul gana pese a inferioridad numerica (multiplicador defensivo)

### Test 3.7 — Modo de agregacion POST

- Repetir Test 3.4 cambiando agregacion a POST
- **Esperado**: Resultados ligeramente diferentes a PRE (POST es mas realista para fuerzas mixtas)

---

## PARTE 4: Pruebas del modo Monte Carlo

### Test 4.1 — Monte Carlo basico

1. Configurar 20 LEOPARDO_2E vs 2 T-80U
2. Seleccionar modo **Monte Carlo**
3. Replicas: 500, Seed: 42
4. Pulsar EJECUTAR SIMULACION
5. **Esperado**:
   - P(Azul gana) cercano a 100%
   - Media de supervivientes azul cercana al resultado determinista
   - Tabla con percentiles p05 < p25 < mediana < p75 < p95

### Test 4.2 — Reproducibilidad

1. Ejecutar Monte Carlo con seed=42
2. Anotar resultados
3. Ejecutar de nuevo con seed=42
4. **Esperado**: Resultados identicos

### Test 4.3 — Monte Carlo con combate equilibrado

1. Configurar 10 LEOPARDO_2E vs 10 T-80U
2. Monte Carlo con 1000 replicas
3. **Esperado**: Distribucion mas repartida entre BLUE_WINS, RED_WINS y DRAW

---

## PARTE 5: Pruebas de parametros avanzados

### Test 5.1 — Fraccion de empenamiento

- 10 vehiculos con engagement_fraction = 0.5
- **Esperado**: Fuerza efectiva = 5, fuerza total = 10

### Test 5.2 — Fraccion de empenamiento por defecto

- 10 vehiculos sin modificar engagement_fraction
- **Esperado**: Fuerza efectiva = 6.67 (defecto 2/3), fuerza total = 10

### Test 5.3 — Combinacion con AFT

- 10 vehiculos, engagement_fraction = 0.5, aft_casualties_pct = 0.2
- **Esperado**: Fuerza efectiva = 4 (10 * (1-0.2) * 0.5)

---

## PARTE 6: Pruebas de terreno

### Test 6.1 — Terreno FACIL vs DIFICIL

Ejecutar el mismo escenario simetrico (10 vs 10) con cada terreno:
- FACIL: combate mas rapido (mayor efectividad de fuego)
- MEDIO: intermedio
- DIFICIL: combate mas lento (menor efectividad de fuego)

En todos los casos, si es simetrico, el resultado debe ser DRAW.

---

## PARTE 7: Problemas frecuentes

| Problema | Causa | Solucion |
|---|---|---|
| La ventana no se abre | Falta SDL2.dll | Verificar que SDL2.dll esta junto al .exe |
| Error de catalogos | Faltan JSON | Copiar vehicle_db.json, vehicle_db_en.json y model_params.json junto al .exe |
| Aviso de Windows Defender | Ejecutable no firmado | Pulsar "Mas informacion" > "Ejecutar de todas formas" |
| Pantalla negra | Driver grafico antiguo | Actualizar drivers de la tarjeta grafica (requiere OpenGL 3.0+) |
| Texto muy pequeno | Pantalla HiDPI | La aplicacion soporta HiDPI automaticamente |

---

## PARTE 8: Verificaciones invariantes

Para **todos** los tests, comprobar siempre:

- `bajas = inicial - supervivientes` (las cuentas cuadran)
- `supervivientes >= 0` (no hay efectivos negativos)
- La ventaja estatica > 1 favorece a azul, < 1 favorece a rojo, = 1 es equilibrado
