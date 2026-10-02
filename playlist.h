#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

struct Song {
    int id;
    std::string title;
};

class Playlist {
private:
    std::vector<Song> songs;
    int current_index;
    int next_id;
    
    // Sincronización para Lectores-Escritores (Readers-Writers)
    std::mutex rw_mutex;
    std::condition_variable rw_cv;
    int readers;
    bool writer;

    void lock_read();
    void unlock_read();
    void lock_write();
    void unlock_write();

public:
    Playlist();
    ~Playlist();

    void addSong(const std::string& title);
    void removeSong(int id);
    void reorderSong(int id, int new_position);
    void clear();
    void printPlaylist();

    bool getCurrentSong(Song& song);
    bool next();
    bool prev();
};

#endif
