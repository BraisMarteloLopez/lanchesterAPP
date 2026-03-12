# Informe de Validacion — Sesion 3

## Resumen

**46 tests ejecutados, 46 pasados, 0 fallidos.**

Se validaron 8 escenarios de prueba cubriendo todos los mecanismos del modelo, mas verificacion de invariantes matematicas en todos los casos. Adicionalmente, se comparo la agregacion pre-tasa (por defecto) con la post-tasa en todos los escenarios.

---

## Tabla de validacion

| Test | Escenario | Verificacion | Resultado |
|---|---|---|---|
| TEST-01 | Simetrico (10 T-80U vs 10 T-80U) | Outcome=DRAW, static_adv=1.0, iniciales=10/10 | OK |
| TEST-02 | Abrumador (20 LEO2E vs 2 T-80U) | BLUE_WINS, supervivientes > 18 | OK |
| TEST-03 | Sin C/C (VEC-25 vs BTR-82A) | Municion C/C = 0 ambos bandos | OK |
| TEST-04 | Fuera de alcance (9000m) | INDETERMINATE, 0 bajas ambos | OK |
| TEST-05 | Bajas AFT 50% | Azul inicia con 5.0, rojo con 10.0 | OK |
| TEST-06 | Defensiva org. media (6 vs 18) | BLUE_WINS (mult 1/4.25^2 compensa inferioridad 1:3) | OK |
| TEST-07 | Fuerzas mixtas | Conteo correcto, ambos consumen C/C | OK |
| TEST-08 | Engagement 0.5 * count 2.0 | Iniciales = 10.0 (10 * 0.5 * 2.0) | OK |

### Invariantes verificados en todos los tests:
- `bajas = inicial - supervivientes` (tolerancia < 0.02)
- `supervivientes >= 0`

---

## Comparativa: Pre-tasa vs Post-tasa

### Casos homogeneos (un solo tipo de vehiculo por bando)

En los tests 01-06 y 08, donde cada bando tiene un unico tipo de vehiculo, **pre-tasa y post-tasa producen resultados identicos**. Esto es correcto: con un solo tipo, no hay agregacion que difiera.

### Caso mixto (TEST-07: LEO2E + PIZARRO vs T-80U + BMP-3)

| Campo | Pre-tasa | Post-tasa | Diferencia |
|---|---|---|---|
| blue_survivors | 1.82 | 0.0 | -1.82 (100%) |
| red_survivors | 0.0 | 4.37 | +4.37 (100%) |
| static_advantage | 0.9988 | 0.9216 | -0.0772 (7.73%) |
| outcome | BLUE_WINS | RED_WINS | **Cambia el vencedor** |

### Analisis de la diferencia

La diferencia es significativa y **cambia el vencedor del combate**. Esto ocurre porque:

1. **Pre-tasa** promedia los parametros de potencia y dureza antes de aplicar la sigmoide `T`. Al mezclar un vehiculo fuerte (LEO2E, P=850) con uno debil (PIZARRO, P=400), la potencia media es ~550, que produce una `T` moderada contra la dureza del T-80U (D=800).

2. **Post-tasa** calcula `T` individualmente para cada tipo. El LEO2E consigue `T` alto contra el T-80U, mientras que el PIZARRO consigue `T` muy bajo. La media ponderada de las tasas resultantes refleja mejor la realidad: los LEO2E hacen casi todo el dano, los PIZARRO poco.

3. La sigmoide es convexa en la zona donde opera este escenario (`P < D`), lo que hace que `T(mean(P)) > mean(T(P))` (desigualdad de Jensen). La agregacion pre-tasa **sobreestima** la efectividad del bando mixto en este regimen.

### Recomendacion

La agregacion **post-tasa es mas realista** para fuerzas con vehiculos muy heterogeneos. Sin embargo, la pre-tasa es la opcion original del modelo y puede ser preferida por compatibilidad. Se recomienda:
- Usar `--aggregation pre` (por defecto) para reproducir el modelo original.
- Usar `--aggregation post` para analisis donde la heterogeneidad de la fuerza sea relevante.
- Documentar que la diferencia puede ser significativa cuando se mezclan vehiculos con potencias muy distintas.

---

## Conclusion

El modelo se comporta correctamente en todos los escenarios probados:
- Las funciones matematicas (T, G, S, S_cc) producen valores coherentes.
- Los multiplicadores tacticos funcionan como se espera.
- El bucle Euler converge y las invariantes se mantienen.
- Los factores de ajuste (AFT, engagement, count_factor, rate_factor) se aplican correctamente.
- La agregacion post-tasa funciona y produce diferencias medibles en escenarios mixtos.
