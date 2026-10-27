#pragma once
#include <cstdint>

// Mono last-note priority stack, max 8 held notes.
class NoteStack {
public:
    void press(int note);
    void release(int note);
    int current() const { return n_ ? notes_[n_ - 1] : -1; }
    void clear() { n_ = 0; }

private:
    int8_t notes_[8];
    uint8_t n_ = 0;
};
