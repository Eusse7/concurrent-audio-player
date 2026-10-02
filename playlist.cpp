#include "playlist.h"
#include <algorithm>
#include <iostream>

Playlist::Playlist() : current_index(-1), next_id(1), readers(0), writer(false) {
}

Playlist::~Playlist() {
}

void Playlist::lock_read() {
    std::unique_lock<std::mutex> lock(rw_mutex);
    while (writer) {
        rw_cv.wait(lock);
    }
    readers++;
}

void Playlist::unlock_read() {
    std::unique_lock<std::mutex> lock(rw_mutex);
    readers--;
    if (readers == 0) {
        rw_cv.notify_all();
    }
}

void Playlist::lock_write() {
    std::unique_lock<std::mutex> lock(rw_mutex);
    while (writer || readers > 0) {
        rw_cv.wait(lock);
    }
    writer = true;
}

void Playlist::unlock_write() {
    std::unique_lock<std::mutex> lock(rw_mutex);
    writer = false;
    rw_cv.notify_all();
}

void Playlist::addSong(const std::string& title) {
    lock_write();
    songs.push_back({next_id++, title});
    if (current_index == -1) {
        current_index = 0;
    }
    unlock_write();
}

void Playlist::removeSong(int id) {
    lock_write();
    auto it = std::find_if(songs.begin(), songs.end(), [id](const Song& s) { return s.id == id; });
    if (it != songs.end()) {
        int index = std::distance(songs.begin(), it);
        songs.erase(it);
        if (index < current_index) {
            current_index--;
        } else if (index == current_index) {
            if (current_index >= (int)songs.size()) {
                current_index = songs.empty() ? -1 : 0;
            }
        }
        std::cout << "Canción eliminada.\n";
    } else {
        std::cout << "ID de canción no encontrado.\n";
    }
    unlock_write();
}

void Playlist::reorderSong(int id, int new_position) {
    lock_write();
    auto it = std::find_if(songs.begin(), songs.end(), [id](const Song& s) { return s.id == id; });
    if (it != songs.end()) {
        int old_index = std::distance(songs.begin(), it);
        Song s = *it;
        songs.erase(it);
        
        if (new_position < 0) new_position = 0;
        if (new_position > (int)songs.size()) new_position = songs.size();
        
        songs.insert(songs.begin() + new_position, s);
        
        if (current_index == old_index) {
            current_index = new_position;
        } else if (old_index < current_index && new_position >= current_index) {
            current_index++;
        } else if (old_index > current_index && new_position <= current_index) {
            current_index--;
        }
        std::cout << "Lista reordenada.\n";
    } else {
        std::cout << "ID de canción no encontrado.\n";
    }
    unlock_write();
}

void Playlist::clear() {
    lock_write();
    songs.clear();
    current_index = -1;
    unlock_write();
    std::cout << "Lista de reproducción vaciada.\n";
}

void Playlist::printPlaylist() {
    lock_read();
    std::cout << "\n--- Playlist ---\n";
    for (size_t i = 0; i < songs.size(); ++i) {
        std::cout << ((int)i == current_index ? "-> " : "   ") 
                  << "[" << songs[i].id << "] " << songs[i].title << "\n";
    }
    if (songs.empty()) {
        std::cout << "  (Lista vacía)\n";
    }
    std::cout << "----------------\n";
    unlock_read();
}

bool Playlist::getCurrentSong(Song& song) {
    bool has_song = false;
    lock_read();
    if (current_index >= 0 && current_index < (int)songs.size()) {
        song = songs[current_index];
        has_song = true;
    }
    unlock_read();
    return has_song;
}

bool Playlist::next() {
    bool can_next = false;
    lock_write();
    if (!songs.empty() && current_index < (int)songs.size() - 1) {
        current_index++;
        can_next = true;
    } else if (!songs.empty()) {
        current_index = 0;
        can_next = true;
    }
    unlock_write();
    return can_next;
}

bool Playlist::prev() {
    bool can_prev = false;
    lock_write();
    if (!songs.empty() && current_index > 0) {
        current_index--;
        can_prev = true;
    } else if (!songs.empty()) {
        current_index = songs.size() - 1;
        can_prev = true;
    }
    unlock_write();
    return can_prev;
}
