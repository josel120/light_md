# light_md

[English](README.md) | **Español** | [Português](README.pt.md) | [Français](README.fr.md) | [Deutsch](README.de.md)

Un editor de Markdown ligero en C++ y Qt 6, abierto a todo tipo de usuarios.

Escribe a la izquierda y ve el resultado formateado a la derecha, al instante. ¿Nunca has usado Markdown? Los botones de arriba dan el formato por ti. ¿Ya lo dominas? Escribe directamente, usa los atajos de teclado y sigue trabajando con archivos `.md` normales.

![light_md: el panel de escritura a la izquierda y el resultado con formato a la derecha](docs/screenshot.png)

**[Descargar light_md para Windows](https://github.com/josel120/light_md/releases/latest)**: descomprime el archivo y abre `bin\light_md.exe`. No hay que instalar nada. Desde la próxima versión también hay un instalador (`windows-x64-setup.exe`): añade light_md al menú Inicio, lo ofrece para los archivos `.md` (en «Abrir con» y cuando Windows pregunta qué aplicación usar) y se desinstala desde Configuración → Aplicaciones. No necesita permisos de administrador.

> La interfaz de la aplicación está en inglés. Los nombres de los botones aparecen abajo tal como se ven en pantalla.

## Qué hace

- Dos paneles: escritura a la izquierda y lectura a la derecha, con vista previa al instante.
- Botones con palabras sencillas: New (nuevo), Open (abrir), Save (guardar), Big title (título grande), Bold (negrita), Bullet list (lista con puntos), Quote (cita), Link (enlace), etc.
- Abre y guarda documentos `.md`, también arrastrándolos a la ventana o con "Abrir con" desde el Explorador de archivos.
- Buscar y reemplazar palabras (Ctrl+F, Ctrl+H), y reabrir tus últimos documentos desde el menú Recent (recientes).
- Varios documentos abiertos en pestañas (Ctrl+N nueva, Ctrl+W cerrar, Ctrl+Tab / Ctrl+Shift+Tab para cambiar), cada uno con su propio deshacer. light_md vuelve a abrir tus pestañas la próxima vez que lo inicies.
- Listas de tareas (Checklist) y un menú Insert (insertar) para imágenes, tablas, bloques de código y líneas separadoras. Las imágenes junto a tu documento se ven en la vista previa y en el PDF.
- Pega una captura de pantalla o cualquier imagen con Ctrl+V: se guarda en una carpeta `images` junto a tu documento y aparece en la vista previa y en el PDF.
- Exporta a PDF e imprime: el resultado se ve igual que el panel derecho.
- Exporta a HTML (una sola página web con las imágenes dentro) o a `.odt` para Word, LibreOffice y Google Docs.
- La interfaz está en español, inglés, portugués, francés y alemán. Sigue el idioma del sistema; cámbialo en **Opciones → Idioma**.
- Deshacer y rehacer, y atajos de teclado (Ctrl+S, Ctrl+Z, Ctrl+B, Ctrl+I, Ctrl+K, Ctrl+1, Ctrl+2).
- Al abrir la app vuelve al último documento, justo donde lo dejaste (cursor y desplazamiento), y recuerda ese punto en cada documento reciente.
- Texto más grande o más pequeño en los dos paneles con Ctrl + rueda del ratón o Ctrl+Más / Ctrl+Menos (Ctrl+0 vuelve al tamaño normal). El tamaño se recuerda.
- No pierdes tu trabajo: unos segundos después de cada cambio, light_md guarda una copia de recuperación. Si la app o el ordenador se cierran de golpe, la próxima vez te ofrece recuperar los cambios sin guardar.
- El panel de escritura se lee mejor: los títulos se ven más grandes, la **negrita** y la *cursiva* se notan, los enlaces y el código destacan y las citas se atenúan. Los símbolos de Markdown siguen visibles, en un color más suave, y los colores siguen el tema.
- Contador de palabras y aviso de cambios sin guardar, también al cerrar la ventana.
- Botón **Check for updates** (buscar actualizaciones): avisa cuando hay una versión nueva y descarga el archivo adecuado para tu sistema.
- **macOS** (13 Ventura o posterior): desde la versión 1.3 hay un `macos.zip` en cada release. Descomprímelo y arrastra **light_md** a Aplicaciones. Como aún no está firmada por Apple, la primera vez macOS la bloquea: abre **Ajustes del Sistema → Privacidad y seguridad** y pulsa **Abrir igualmente**.
- Selector de tema: **System** (escritorio), **Minimal** (aspecto claro y sobrio), **Dark** (oscuro y suave, Ctrl+Shift+4), **Omarchy** (sigue el tema Omarchy en Linux).
- Atajos: **Ctrl+Shift+B** mostrar u ocultar los botones, **Ctrl+E** editar, **Ctrl+R** / **Esc** solo lectura; el resto está en Options (ver tabla en [README.md](README.md)).
- Botón Help (ayuda) con una guía corta.
- Corrector ortográfico en inglés, español, portugués, francés y alemán: las palabras mal escritas llevan una onda roja, y con clic derecho se ofrecen correcciones o se añade la palabra a tu diccionario. El código y las direcciones web no se revisan. Elige el idioma o apágalo en **Opciones → Ortografía**.
- Panel de **índice** a la izquierda: todos los títulos y subtítulos, anidados. Haz clic en uno para saltar a él en los dos paneles; se actualiza mientras escribes y marca la sección en la que estás. Ocúltalo con **Ctrl+Shift+O**.
- Modo **sin distracciones** (**F11**): pantalla completa solo con tu texto, en una columna cómoda en el centro. Esc o F11 lo devuelve todo.

Consulta [ROADMAP.md](ROADMAP.md) (en inglés) para ver qué está hecho y qué viene después.

## Compilar en Linux

Hace falta Qt 6 (`Widgets` y `PrintSupport`). En Arch / [Omarchy](https://omarchy.org/):

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-tools qt6-wayland
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/light_md
```

Para instalarlo en tu usuario (binario, entrada `.desktop` e icono para el lanzador):

```bash
cmake --install build --prefix ~/.local
```

En el primer arranque bajo Omarchy, el tema por defecto es **Omarchy** y se actualiza al cambiar de tema. En el resto de sistemas, el valor inicial es **System**. La elección se recuerda.

## Compilar (Windows, con Qt 6 y MinGW)

Necesitas Qt 6 instalado (por ejemplo en `C:\Qt`) con su CMake y Ninja.

```powershell
$env:PATH = "C:\Qt\Tools\mingw1510_64\bin;C:\Qt\6.12.1\mingw_64\bin;$env:PATH"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.1/mingw_64
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build
.\build\light_md.exe
```

Ajusta las rutas si tu versión de Qt o de MinGW es otra.

## Llevarlo a otro equipo (Windows)

Para crear una carpeta que funcione en cualquier PC con Windows sin instalar Qt:

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe --install build --prefix dist
```

Copia la carpeta `dist` completa y abre `dist\bin\light_md.exe`.

## Licencia

MIT

El seguimiento del tema Omarchy sigue el enfoque de Omacut (MIT, David Heinemeier Hansson).
