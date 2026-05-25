#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>
#include <string>
#include <vector>
#include <memory>

// Forward declarations
class AutocompleteEngine;

/**
 * TerminalEditor - A simple text editor with autocomplete
 *
 * Features:
 * - Multi-line editing
 * - Real-time autocomplete suggestions
 * - Syntax highlighting (optional)
 * - Status bar with info
 * - Vim-like key bindings
 *
 * Controls:
 * - Arrow keys: Move cursor
 * - Ctrl+S: Save file
 * - Ctrl+Q: Quit
 * - Tab: Accept autocomplete suggestion
 * - Ctrl+Space: Trigger autocomplete manually
 * - Esc: Close autocomplete popup
 */

class TerminalEditor {
private:
    std::vector<std::string> lines;
    int cursorX, cursorY;
    int scrollY;
    int screenHeight, screenWidth;

    WINDOW* mainWin;
    WINDOW* statusWin;
    WINDOW* autocompleteWin;

    std::string filename;
    bool modified;
    bool showAutocomplete;
    std::vector<std::string> suggestions;
    int selectedSuggestion;

    std::unique_ptr<AutocompleteEngine> engine;
    std::string currentWord;

    void initWindows();
    void drawEditor();
    void drawStatusBar();
    void drawAutocompleteSuggestions();
    void updateCurrentWord();
    void triggerAutocomplete();
    void acceptSuggestion();
    void insertChar(char c);
    void deleteChar();
    void newLine();
    void moveCursor(int dx, int dy);
    void saveFile();
    std::string getCurrentLine();

public:
    TerminalEditor(const std::string& file = "");
    ~TerminalEditor();

    void run();
    void loadFile(const std::string& file);
};

#endif
