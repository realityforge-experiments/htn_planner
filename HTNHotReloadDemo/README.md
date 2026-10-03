# HTNHotReloadDemo

Playground aislado de hot reload del dominio Wanderer, reutilizando los NPC,
terreno y panel de simulación de HTNDemo. Soporta Windows x64 con VS2022 C++
(incluido Windows SDK) y Linux x86_64 con GCC. No modifica la ABI del planner.

## Arranque en Windows

1. Ejecutar `GenerateProjectFiles.bat` para regenerar la solución VS2022.
   Si usabas opciones de Premake, mantenerlas al regenerar.
2. Seleccionar `HTNHotReloadDemo` como proyecto de inicio y compilar en `Profile`
   (también admite Debug, ProfileDetailed y Release). Las dependencias generan
   el primer dominio y compilan el translator y el runtime bridge.
3. Ejecutar el proyecto. Deben aparecer ocho NPC y el estado `Initial build active`.

No hacer Build/Rebuild desde Visual Studio con la demo abierta: la DLL inicial
está cargada y Windows impide sobrescribirla. Para iterar, usar sus botones.

## Arranque en Linux / Ubuntu WSL2

Desde el checkout Linux, con las dependencias de [LINUX_SMOKE.md](../docs/LINUX_SMOKE.md):

```sh
PREMAKE5=/path/to/premake5 bash BuildAndTestLinux.sh Debug
./bin/Debug-linux-x86_64/HTNHotReloadDemo/HTNHotReloadDemo
```

La compilación inicial genera `libWandererHTN.so` y copia `libHTNRuntimeBridge.so`
junto al ejecutable. El botón Compile utiliza `HTNHotReloadDemo/CompileDomain.sh`,
el translator de la misma configuración y `${CC:-gcc-14}`. El ejecutable pasa sus
macros de instrumentación al script para mantener la compatibilidad con el host.
Si quieres otro compilador, configura `CC` antes de arrancar el demo. El script
compila C11 con PIC y errores ante símbolos sin resolver; el módulo busca el
bridge junto a sí mismo mediante `$ORIGIN` después de aplicarse la recarga.

Cierra el demo antes de ejecutar builds o self-tests, también en Linux. La prueba
automática se ejecuta sin ventana; el uso interactivo requiere un entorno gráfico.
El SDK facilita la ejecución de módulos generados; este demo implementa la
compilación, carga, recarga y recuperación que corresponden al cliente.

## Flujo

- `Domain Source`: editar `Domains/Wanderer.domain` y pulsar `Save Domain`.
  `Discard edits / Read from disk` vuelve a leer el fichero. Los includes, como
  `Domains/Includes/movement2.domain`, se pueden editar externamente.
- `Compile Domain`: ejecuta HTNTranslator de la misma configuración y después
  el compilador C sobre el código generado. Produce un módulo candidato **de
  nombre fijo**: `candidate/WandererHTN.dll` en Windows o
  `candidate/libWandererHTN.so` en Linux, dentro del directorio del ejecutable.
  Los NPC continúan usando el módulo activo. El resultado y los diagnósticos se
  muestran en `Compiler Output` (la vista limita el texto a 256 KiB; el log en
  `build/logs/hot-reload-<config>-compiler.log` conserva la salida completa).
- `Hot Reload`: entre updates, libera las unidades y el almacenamiento preparado
  de todos los NPC, descarga el módulo activo, sustituye su archivo con el
  candidato y valida/carga la nueva definición. **No ejecuta HTNTranslator**.
  Solo aplica el último candidato compilado correctamente. Para incluir cambios
  externos posteriores hay que volver a pulsar Compile.
- Conserva posiciones, edad, velocidad, estado de juego, WorldState, hooks,
  daemons, selección y estadísticas históricas. Descarta los planes activos,
  temporizadores de primitives y capturas del debugger. Replanifica en el
  siguiente update; el scratch de ejecución de cada unidad se inicializa entonces.
- Antes de descargar guarda una copia fija en `previous/<nombre del módulo>`.
  Si falla la recarga, restaura ese módulo y reconstruye los planners. Si falla la
  restauración o no había dominio activo, mantiene la simulación sin planner y
  pausada hasta que se compile/recargue un módulo válido. Un fallo al hacer backup
  deja intacto el dominio activo.

Guardar cambios invalida el candidato; con cambios sin guardar los botones están
deshabilitados. Compilar no ejecuta callterms del candidato. La recarga comprueba
ABI/export y lifecycle; no demuestra que el nuevo dominio encuentre un plan con
todos los WorldStates. Un fallo de planificación se observa en el panel de NPC.

El runtime bridge permanece cargado durante toda la sesión. Solo se descarga la
biblioteca del dominio. El binding global se configura una vez; cada NPC sigue teniendo
su propio contexto de daemons y PlannerHook. No se usa el intérprete en esta demo.

## Comprobación rápida

Desde la raíz, una vez compilado Profile:

```bat
bin\Profile-windows-x86_64\HTNHotReloadDemo\HTNHotReloadDemo.exe --self-test
```

En Linux Debug:

```sh
./bin/Debug-linux-x86_64/HTNHotReloadDemo/HTNHotReloadDemo --self-test
```

Comprueba con los módulos reales ocho recargas **durante una primitive de movimiento,
con deferred calls pendientes**, conservación de WorldState/posición/
edad/velocidad, eliminación del plan anterior, replanning, rollback ante un módulo
inválido y update seguro sin dominio. No abre ventana ni modifica los domains;
solo usa artefactos del directorio de binarios. Guarda el módulo original en
`self-test-original/<nombre del módulo>` y lo restaura al terminar, incluso ante un
fallo normal de las comprobaciones. Si falla la copia de restauración, devuelve
error y muestra la ruta desde la que recuperarla. Cerrar
otras instancias de la demo antes de ejecutarlo. Debe terminar con
`Hot reload self-test: PASS` y código 0.

También exige facts publicados al inicializar y movimiento/ejecución real de tasks:
que la rama fallback idle devuelva un plan válido no basta para pasar. La demo
registra al crear cada hook los siete facts que publican sus daemons, sin depender
de parsear el source editable. Ese registro permanece válido durante las recargas.

## Validación completa de Compile + Hot Reload

```bat
bin\Profile-windows-x86_64\HTNHotReloadDemo\HTNHotReloadDemo.exe --pipeline-self-test
```

En Linux Debug, desde la raíz (también incluido en `BuildAndTestLinux.sh`):

```sh
mkdir -p build/logs
./bin/Debug-linux-x86_64/HTNHotReloadDemo/HTNHotReloadDemo --pipeline-self-test \
    > build/logs/linux-reload-manual-Debug-pipeline.log 2>&1
```

Incluye la comprobación anterior y usa **la misma ruta asíncrona de Compile que
el botón**, con un source de prueba aislado en `pipeline fixture's/Wanderer.domain`.
El nombre comprueba también el manejo de espacios y apóstrofos:

1. Copia el dominio de `HTNHotReloadDemo/Validation` y el include de movimiento al
   directorio de binarios. No modifica tus sources originales.
2. Compila la revisión A y luego B. Comprueba que Compile no cambia la definición,
   revisión ni contenido del módulo activo y que los NPC siguen actualizándose.
3. Verifica que el comportamiento nuevo solo aparece después de Hot Reload,
   mediante las primitives `!say "reload-validation-A"` y `!say "reload-validation-B"`.
4. Introduce un error de sintaxis en la copia. Exige un fallo del compilador,
   candidato no disponible, Hot Reload rechazado y dominio/NPC activos intactos.
5. Recarga ocho veces durante movimiento con deferred calls pendientes, verifica
   que el movimiento se retoma y que se ejecuta el comentario deferred de B
   **después** de esas recargas. Prueba también rollback y movimiento posterior.
6. Descarga los módulos y restaura el módulo original antes de devolver éxito.

Salida final esperada: `Hot reload pipeline self-test: PASS`, código 0. Los errores
del compilador durante la prueba de source inválido son intencionados; no deben
hacer fallar el test. No requiere ventana ni GitHub Actions. Ejecutar con todas
las instancias de la demo cerradas y sin builds concurrentes. La prueba espera
a que termine el compilador; no tiene timeout/cancelación.

El `--self-test` básico no necesita invocar al compilador. En Windows los tests
suprimen el diálogo del sistema para imágenes inválidas: esos módulos son fixtures
intencionados y sus errores se comprueban a través del loader.

Prueba manual: cambiar un texto de `!say`, guardar, Compile (el dominio activo no
cambia), Hot Reload y comprobar el nuevo texto cuando se alcance esa rama. Luego
introducir un error de sintaxis y comprobar que Compile falla sin interrumpir los
NPC. Deshacer el error y repetir.

## Límites deliberados

Sin watcher, recarga automática, migración de planes ni compilación remota. El
build corre en un hilo; cerrar la aplicación espera a que termine, sin timeout ni
cancelación. Cambiar el ABI/runtime, flags de instrumentación o firmas/bindings
de C++ exige cerrar y recompilar los proyectos correspondientes, no solo el domain.
No es una transacción resistente a caídas del proceso ni un sandbox para módulos
no confiables. Una falta de memoria en la preparación por NPC se rige por los
mecanismos de allocation existentes del planner; no se añade otra capa de allocator.
