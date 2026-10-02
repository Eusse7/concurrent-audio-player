# Reproductor de Audio Concurrente 🎵
Un reproductor de música de terminal desarrollado en C++11 que aplica conceptos avanzados de Sistemas Operativos.

## 🚀 Características
- **Arquitectura Productor-Consumidor**: Sincronización perfecta entre la decodificación del archivo (Productor) y la tarjeta de sonido (Consumidor).
- **Audio Real**: Utiliza la librería `miniaudio` para decodificar MP3/WAV y comunicarse directamente con PulseAudio/ALSA en Linux.
- **Búfer Circular Robusto**: Almacena de forma eficiente los *frames* de audio y mitiga la latencia utilizando `std::mutex` y `std::condition_variable`.
- **Listas de Reproducción Thread-Safe**: Controladas mediante candados de Lectura/Escritura (RW Locks).

## 🛠️ Compilación y Ejecución
Este proyecto fue diseñado para entornos Linux (POSIX).

```bash
make
./reproductor
```
