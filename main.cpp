#include <iostream>
#include <string>
#include "playlist.h"
#include "audio_engine.h"

void printMenu() {
    std::cout << "\n=== Reproductor de Audio Concurrente ===\n"
              << "Comandos:\n"
              << "  play          - Iniciar/reanudar reproducción\n"
              << "  pause         - Pausar reproducción\n"
              << "  stop          - Detener reproducción\n"
              << "  next          - Siguiente canción\n"
              << "  prev          - Canción anterior\n"
              << "  add <nombre>  - Añadir canción a la lista\n"
              << "  rm <id>       - Eliminar canción por ID\n"
              << "  mv <id> <pos> - Mover canción a una nueva posición (0-index)\n"
              << "  ls            - Mostrar lista de reproducción\n"
              << "  clear         - Vaciar lista de reproducción\n"
              << "  v             - Mostrar/Ocultar Visualizador de Espectro\n"
              << "  exit          - Salir\n"
              << "> " << std::flush;
}

int main() {
    Playlist playlist;
    AudioEngine* engine = new AudioEngine(&playlist);

    // Default songs
    // playlist.addSong("Bohemian Rhapsody");
    // playlist.addSong("Hotel California");
    // playlist.addSong("Stairway to Heaven");

    engine->start();

    std::string cmd;
    printMenu();
    while (std::cin >> cmd) {
        if (cmd == "play") {
            engine->play();
            std::cout << "Reproduciendo...\n> " << std::flush;
        } else if (cmd == "pause") {
            engine->pause();
            std::cout << "Pausado.\n> " << std::flush;
        } else if (cmd == "stop") {
            engine->stop();
            std::cout << "Detenido.\n> " << std::flush;
        } else if (cmd == "next") {
            engine->next();
            std::cout << "Saltando a la siguiente...\n> " << std::flush;
        } else if (cmd == "prev") {
            engine->prev();
            std::cout << "Saltando a la anterior...\n> " << std::flush;
        } else if (cmd == "add") {
            std::string name;
            std::getline(std::cin, name);
            if (!name.empty() && name[0] == ' ') name.erase(0, 1);
            playlist.addSong(name);
            std::cout << "Canción '" << name << "' añadida.\n> " << std::flush;
        } else if (cmd == "rm") {
            int id;
            if (std::cin >> id) {
                playlist.removeSong(id);
                std::cout << "> " << std::flush;
            }
        } else if (cmd == "mv") {
            int id, pos;
            if (std::cin >> id >> pos) {
                playlist.reorderSong(id, pos);
                std::cout << "> " << std::flush;
            }
        } else if (cmd == "ls") {
            playlist.printPlaylist();
            std::cout << "> " << std::flush;
        } else if (cmd == "clear") {
            playlist.clear();
            std::cout << "> " << std::flush;
        } else if (cmd == "v") {
            engine->toggleVisualizer();
            if (engine->isVisualizerActive()) {
                std::cout << "\033[2J\033[H"; // Limpiar consola
            } else {
                std::cout << "\033[11B\n\n"; // Mover cursor abajo para no sobreescribir y reimprimir
                printMenu();
            }
        } else if (cmd == "exit") {
            break;
        } else if (cmd == "help") {
            printMenu();
        } else {
            std::cout << "Comando desconocido. Escribe 'help' para ver los comandos.\n> " << std::flush;
        }
    }

    std::cout << "Saliendo del reproductor...\n";
    engine->stopEngine();
    delete engine;
    return 0;
}
