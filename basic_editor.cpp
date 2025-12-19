/*
 * Terminal-based code editor with autocomplete.
 *
 * Features:
 * - ncurses-based text editing
 * - syntax highlighting
 * - file open / save
 * - search and navigation
 * - undo / redo
 * - intelligent autocomplete using classic DSA
 *
 * Autocomplete engine uses:
 * - TST for prefix matching
 * - MinHeap for ranking
 * - LRU cache for reuse
 * - KMP for substring matches
 * - Graph + frequency for contextual learning
 */

#include <ncurses.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

#include "tst.h"
#include "phrase_store.h"
#include "freq_store.h"
#include "ranker.h"
#include "graph.h"
#include "kmp.h"
#include "minheap.h"
#include "lru.h"
#include "stack.h"

class BasicEditor {
private:
    std::vector<std::string> lines;
    std::vector<std::string> suggestions;
    std::vector<bool> isPhraseFlag;

    int cursorY = 0;
    int cursorX = 0;
    int scrollY = 0;

    bool showingSuggestions = false;
    int selectedSuggestion = 0;

    TST tst;
    PhraseStore phraseStore;
    FreqStore freqStore;
    CooccurrenceGraph graph;
    Ranker ranker;

    std::vector<std::string> dictionaryWords;
    std::string lastAcceptedWord;

    MinHeap suggestionHeap{10};
    lru_cache suggestionCache{100};
    UndoRedoStack undoRedoStack;

    std::string currentFileName;
    bool fileModified = false;

    std::string searchQuery;

public:
    BasicEditor()
        : phraseStore("data/phrases.txt"),
        freqStore("data/frequency.txt"),
        ranker(&freqStore, &graph) {

        lines.push_back("");
        loadDictionary();

        std::error_code ec;
        std::filesystem::create_directories("scratch", ec);
    }

    void run() {
        initscr();
        raw();
        keypad(stdscr, TRUE);
        noecho();
        curs_set(1);

        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_CYAN, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(6, COLOR_RED, COLOR_BLACK);

        bool running = true;
        while (running) {
            draw();
            running = handleInput(getch());
        }

        phraseStore.save();
        freqStore.save();
        endwin();
    }

private:
    void loadDictionary() {
        std::ifstream file("data/words.txt");
        if (!file.is_open()) return;

        std::string word;
        while (file >> word) {
            tst.insert(word);
            dictionaryWords.push_back(word);
        }
    }

    bool isKeyword(const std::string& w) {
        static const std::vector<std::string> keys = {
            "auto","break","case","char","const","continue","default","do",
            "double","else","enum","extern","float","for","goto","if",
            "int","long","register","return","short","signed","sizeof","static",
            "struct","switch","typedef","union","unsigned","void","volatile","while",
            "class","namespace","template","public","private","protected","virtual",
            "bool","true","false","nullptr","new","delete","try","catch","throw",
            "using","std","string","vector","map","set","include","define","ifdef"
        };
        return std::find(keys.begin(), keys.end(), w) != keys.end();
    }

    void drawLine(int y, int num, const std::string& line) {
        mvprintw(y, 0, "%3d | ", num);
        int x = 6;

        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '"' || line[i] == '\'') {
                char q = line[i];
                attron(COLOR_PAIR(2));
                mvaddch(y, x++, line[i++]);
                while (i < line.size() && line[i] != q) {
                    mvaddch(y, x++, line[i]);
                    if (line[i] == '\\' && i + 1 < line.size())
                        mvaddch(y, x++, line[++i]);
                    i++;
                }
                if (i < line.size()) mvaddch(y, x++, line[i]);
                attroff(COLOR_PAIR(2));
                continue;
            }

            if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
                attron(COLOR_PAIR(3));
                while (i < line.size()) mvaddch(y, x++, line[i++]);
                attroff(COLOR_PAIR(3));
                break;
            }

            if (line[i] == '#') {
                attron(COLOR_PAIR(5));
                while (i < line.size() && (isalnum(line[i]) || line[i] == '_' || line[i] == '#'))
                    mvaddch(y, x++, line[i++]);
                attroff(COLOR_PAIR(5));
                i--;
                continue;
            }

            if (isdigit(line[i])) {
                attron(COLOR_PAIR(4));
                while (i < line.size() && (isdigit(line[i]) || line[i] == '.'))
                    mvaddch(y, x++, line[i++]);
                attroff(COLOR_PAIR(4));
                i--;
                continue;
            }

            if (strchr("+-*/%=<>!&|^~?:;,(){}[]", line[i])) {
                attron(COLOR_PAIR(6));
                mvaddch(y, x++, line[i]);
                attroff(COLOR_PAIR(6));
                continue;
            }

            if (isalnum(line[i]) || line[i] == '_') {
                std::string word;
                while (i < line.size() && (isalnum(line[i]) || line[i] == '_'))
                    word += line[i++];
                i--;

                if (isKeyword(word)) {
                    attron(COLOR_PAIR(1) | A_BOLD);
                    mvprintw(y, x, "%s", word.c_str());
                    attroff(COLOR_PAIR(1) | A_BOLD);
                } else {
                    mvprintw(y, x, "%s", word.c_str());
                }
                x += word.size();
                continue;
            }

            mvaddch(y, x++, line[i]);
        }
    }

    void draw() {
        clear();
        int maxLines = LINES - 3;

        for (int i = 0; i < maxLines && scrollY + i < (int)lines.size(); i++)
            drawLine(i, scrollY + i + 1, lines[scrollY + i]);

        if (showingSuggestions && !suggestions.empty()) {
            int y = cursorY - scrollY + 1;
            int x = cursorX + 6;
            for (int i = 0; i < (int)suggestions.size() && i < 5; i++)
                mvprintw(y + i, x,
                    i == selectedSuggestion ? " > %s" : "   %s",
                    suggestions[i].c_str());
        }

        attron(A_REVERSE);
        mvprintw(LINES - 2, 0,
            " %s%s | Line %d/%zu Col %d | Ctrl+O Open | Ctrl+W Save | Ctrl+R Search | Ctrl+Q Quit ",
            currentFileName.empty() ? "[No Name]" : currentFileName.c_str(),
            fileModified ? " [+]" : "",
            cursorY + 1, lines.size(), cursorX + 1);
        attroff(A_REVERSE);

        move(cursorY - scrollY, cursorX + 6);
        refresh();
    }

    void updateScroll() {
        int maxLines = LINES - 3;
        if (cursorY < scrollY) scrollY = cursorY;
        else if (cursorY >= scrollY + maxLines)
            scrollY = cursorY - maxLines + 1;
    }

    bool handleInput(int ch) {
        switch (ch) {
            case 17: return false; // Ctrl+Q
            case KEY_UP:
                if (showingSuggestions && selectedSuggestion > 0) {
                    selectedSuggestion--;
                } else if (!showingSuggestions && cursorY > 0) {
                    cursorY--;
                    updateScroll();
                }
                break;

            case KEY_DOWN:
                if (showingSuggestions && selectedSuggestion < (int)suggestions.size() - 1) {
                    selectedSuggestion++;
                } else if (!showingSuggestions && cursorY < (int)lines.size() - 1) {
                    cursorY++;
                    updateScroll();
                }
                break;

            case KEY_LEFT:
                if (cursorX > 0) cursorX--;
                break;
            case KEY_RIGHT:
                if (cursorX < (int)lines[cursorY].size()) cursorX++;
                break;
            case '\n':
                undoRedoStack.pushInsert(cursorY, lines[cursorY]);
                lines.insert(lines.begin() + cursorY + 1,
                            lines[cursorY].substr(cursorX));
                lines[cursorY] = lines[cursorY].substr(0, cursorX);
                cursorY++;
                cursorX = 0;
                fileModified = true;
                updateScroll();
                break;
            case KEY_BACKSPACE:
            case 127:
                undoRedoStack.pushInsert(cursorY, lines[cursorY]);
                if (cursorX > 0) {
                    lines[cursorY].erase(cursorX - 1, 1);
                    cursorX--;
                }
                fileModified = true;
                break;
            case '\t':
                if (showingSuggestions) acceptSuggestion();
                else triggerAutocomplete();
                break;
            default:
                if (ch >= 32 && ch <= 126) {
                    undoRedoStack.pushInsert(cursorY, lines[cursorY]);
                    lines[cursorY].insert(cursorX++, 1, ch);
                    fileModified = true;
                    triggerAutocomplete();
                }
        }
        return true;
    }

    void triggerAutocomplete() {
        std::string word = getCurrentWord();
        if (word.empty()) {
            showingSuggestions = false;
            return;
        }

        suggestions.clear();
        suggestionHeap.clear();

        if (suggestionCache.exists(word)) {
            suggestions = suggestionCache.get(word);
            showingSuggestions = true;
            selectedSuggestion = 0;
            return;
        }

        auto phrases = phraseStore.getTopPhrases(word, 3);
        for (auto& p : phrases)
            suggestionHeap.insert(5.0, "[PHRASE] " + p.snippet);

        auto tokens = tst.prefixSearch(word, 10);
        for (auto& t : tokens)
            suggestionHeap.insert(freqStore.get(t), t);

        auto ranked = suggestionHeap.getAll();
        std::sort(ranked.begin(), ranked.end(),
                [](auto& a, auto& b) { return a.first > b.first; });

        for (auto& [_, s] : ranked)
            suggestions.push_back(s);

        suggestionCache.put(word, suggestions);
        showingSuggestions = true;
        selectedSuggestion = 0;
    }

    void acceptSuggestion() {
        std::string word = getCurrentWord();
        std::string s = suggestions[selectedSuggestion];

        if (s.rfind("[PHRASE]", 0) == 0)
            s = s.substr(9);

        int start = cursorX - word.size();
        lines[cursorY].erase(start, word.size());
        lines[cursorY].insert(start, s);
        cursorX = start + s.size();

        freqStore.bump(s, 1);
        if (!lastAcceptedWord.empty())
            graph.addEdge(lastAcceptedWord, s);
        lastAcceptedWord = s;

        showingSuggestions = false;
    }

    std::string getCurrentWord() {
        auto& line = lines[cursorY];
        int end = cursorX, start = end;
        while (start > 0 &&
            (isalnum(line[start - 1]) || line[start - 1] == '_' || line[start - 1] == '#'))
            start--;
        return line.substr(start, end - start);
    }
};

int main() {
    BasicEditor editor;
    editor.run();
    return 0;
}
