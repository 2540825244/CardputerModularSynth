#include "note_stack.h"

void NoteStack::release(int note) {
    for (uint8_t i = 0; i < n_; i++) {
        if (notes_[i] == note) {
            for (uint8_t j = i; j + 1 < n_; j++) notes_[j] = notes_[j + 1];
            n_--;
            return;
        }
    }
}

void NoteStack::press(int note) {
    release(note);  // no duplicates; re-press moves the note to the top
    if (n_ == 8) {
        for (uint8_t j = 0; j + 1 < 8; j++) notes_[j] = notes_[j + 1];
        n_--;
    }
    notes_[n_++] = (int8_t)note;
}
