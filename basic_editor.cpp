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

#include <chrono>

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
    std::string statusMessage;
    long lastLatencyUs = 0;

    // Wider than the 10 we display: the trie cuts alphabetically, so a
    // frequent match sitting 11th would never reach the heap.
    static constexpr int CANDIDATE_LIMIT = 50;
    static constexpr int MAX_SUGGESTIONS = 10;

    // Learned phrases outrank raw frequency; useCount breaks ties.
    static constexpr double PHRASE_BASE_SCORE = 100.0;

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
            " %s%s | Line %d/%zu Col %d | Tab Complete | Ctrl+S Save | Ctrl+Z Undo | Ctrl+Y Redo | Ctrl+Q Quit ",
            currentFileName.empty() ? "[No Name]" : currentFileName.c_str(),
            fileModified ? " [+]" : "",
            cursorY + 1, lines.size(), cursorX + 1);
        attroff(A_REVERSE);

        move(LINES - 1, 0);
        clrtoeol();
        if (!statusMessage.empty())
            mvprintw(LINES - 1, 0, "%s", statusMessage.c_str());
        else if (lastLatencyUs > 0)
            mvprintw(LINES - 1, 0, "autocomplete: %ld us", lastLatencyUs);

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
        statusMessage.clear();
        switch (ch) {
            case 17: return false; // Ctrl+Q
            case 19:               // Ctrl+S
                saveFile();
                break;
            case 26:               // Ctrl+Z
                performUndo();
                break;
            case 25:               // Ctrl+Y
                performRedo();
                break;
            case 27:               // Esc-dismiss the popup
                showingSuggestions = false;
                break;
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
                snapshotDocument();
                showingSuggestions = false;
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
                snapshotDocument();
                if (cursorX > 0) {
                    lines[cursorY].erase(cursorX - 1, 1);
                    cursorX--;
                } else if (cursorY > 0) {
                    // Join with the previous line.
                    cursorX = (int)lines[cursorY - 1].size();
                    lines[cursorY - 1] += lines[cursorY];
                    lines.erase(lines.begin() + cursorY);
                    cursorY--;
                    updateScroll();
                }
                fileModified = true;
                triggerAutocomplete();
                break;
            case '\t':
                if (showingSuggestions) acceptSuggestion();
                else triggerAutocomplete();
                break;
            default:
                if (ch >= 32 && ch <= 126) {
                    snapshotDocument();
                    lines[cursorY].insert(cursorX++, 1, ch);
                    fileModified = true;
                    triggerAutocomplete();
                }
        }
        return true;
    }

    void triggerAutocomplete() {
        auto t0 = std::chrono::high_resolution_clock::now();
        std::string word = getCurrentWord();
        if (word.empty()) {
            showingSuggestions = false;
            return;
        }

        suggestions.clear();

        if (suggestionCache.exists(word)) {
            suggestions = suggestionCache.get(word);
            if (!suggestions.empty()) {
                showingSuggestions = true;
                selectedSuggestion = 0;
                recordLatency(t0);
                return;
            }
        }

        suggestionHeap.clear();
        std::unordered_set<std::string> seen;

        // The graph boost is relative to the last accepted token.
        ranker.setLastToken(lastAcceptedWord);

        // 1) Learned phrases for this trigger.
        for (const auto& p : phraseStore.getTopPhrases(word, 3)) {
            if (!seen.insert(p.snippet).second) continue;
            suggestionHeap.insert(PHRASE_BASE_SCORE + p.useCount,
                                  "[PHRASE] " + p.snippet);
        }

        // 2) Prefix matches, scored by the Ranker (frequency + graph boost).
        for (const auto& t : tst.prefixSearch(word, CANDIDATE_LIMIT)) {
            if (!seen.insert(t).second) continue;
            suggestionHeap.insert(ranker.computeScore(t), t);
        }

        // 3) KMP substring fallback ("rint" -> "printf"). Linear scan, so it
        //    only runs when prefix matching came up short.
        if (suggestionHeap.size() < MAX_SUGGESTIONS) {
            for (const auto& w : dictionaryWords) {
                if (suggestionHeap.size() >= MAX_SUGGESTIONS) break;
                if (w.rfind(word, 0) == 0) continue;      // already a prefix hit
                if (seen.count(w)) continue;
                if (KMP::contains(w, word)) {
                    seen.insert(w);
                    suggestionHeap.insert(ranker.computeScore(w), w);
                }
            }
        }

        // getAll() already returns highest-score-first.
        for (const auto& [score, s] : suggestionHeap.getAll())
            suggestions.push_back(s);

        if (!suggestions.empty())
            suggestionCache.put(word, suggestions);

        showingSuggestions = !suggestions.empty();
        selectedSuggestion = 0;
        recordLatency(t0);
    }

    void recordLatency(std::chrono::high_resolution_clock::time_point t0) {
        auto t1 = std::chrono::high_resolution_clock::now();
        lastLatencyUs =
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    void acceptSuggestion() {
        if (!showingSuggestions || suggestions.empty()) return;
        if (selectedSuggestion < 0 ||
            selectedSuggestion >= (int)suggestions.size()) return;

        snapshotDocument();

        std::string word = getCurrentWord();
        std::string s = suggestions[selectedSuggestion];

        if (s.rfind("[PHRASE] ", 0) == 0)
            s = s.substr(9);

        int start = cursorX - (int)word.size();
        if (start < 0) start = 0;
        lines[cursorY].erase(start, word.size());
        lines[cursorY].insert(start, s);
        cursorX = start + (int)s.size();

        freqStore.bump(s, 1);
        if (!lastAcceptedWord.empty())
            graph.addEdge(lastAcceptedWord, s);
        lastAcceptedWord = s;

        // Frequency and graph edges just changed, so cached lists are stale.
        // Accepts are rare next to keystrokes, so clearing all is fine.
        suggestionCache.clear();

        fileModified = true;
        showingSuggestions = false;
    }

    // Unnamed buffers default into scratch/, which the constructor creates.
    void saveFile() {
        if (currentFileName.empty()) {
            echo();
            move(LINES - 1, 0);
            clrtoeol();
            mvprintw(LINES - 1, 0, "Save as: ");
            char buf[256] = {0};
            getnstr(buf, 255);
            noecho();

            if (buf[0] == '\0') {
                statusMessage = "Save cancelled";
                return;
            }
            currentFileName = buf;
            if (currentFileName.find('/') == std::string::npos)
                currentFileName = "scratch/" + currentFileName;
        }

        std::ofstream out(currentFileName);
        if (!out) {
            statusMessage = "Could not write " + currentFileName;
            return;
        }
        for (const auto& line : lines)
            out << line << "\n";

        fileModified = false;
        statusMessage = "Saved " + currentFileName;
    }

    // Undo/redo snapshots the whole document: Enter and a line-joining
    // Backspace change the line count, which a per-line snapshot cannot undo.
    std::string serializeDocument() const {
        std::string out;
        for (size_t i = 0; i < lines.size(); i++) {
            if (i) out += '\n';
            out += lines[i];
        }
        return out;
    }

    void restoreDocument(const std::string& snap) {
        lines.clear();
        std::string cur;
        for (char c : snap) {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else cur += c;
        }
        lines.push_back(cur);
        if (lines.empty()) lines.push_back("");

        if (cursorY >= (int)lines.size()) cursorY = (int)lines.size() - 1;
        if (cursorY < 0) cursorY = 0;
        if (cursorX > (int)lines[cursorY].size()) cursorX = (int)lines[cursorY].size();
    }

    void snapshotDocument() {
        undoRedoStack.pushInsert(cursorY, serializeDocument());
    }

    void performUndo() {
        if (!undoRedoStack.canUndo()) {
            statusMessage = "Nothing to undo";
            return;
        }
        auto [y, snap] = undoRedoStack.undo(serializeDocument());
        restoreDocument(snap);
        cursorY = std::min(std::max(y, 0), (int)lines.size() - 1);
        cursorX = std::min(cursorX, (int)lines[cursorY].size());
        showingSuggestions = false;
        fileModified = true;
        statusMessage = "Undo";
        updateScroll();
    }

    void performRedo() {
        if (!undoRedoStack.canRedo()) {
            statusMessage = "Nothing to redo";
            return;
        }
        auto [y, snap] = undoRedoStack.redo(serializeDocument());
        restoreDocument(snap);
        cursorY = std::min(std::max(y, 0), (int)lines.size() - 1);
        cursorX = std::min(cursorX, (int)lines[cursorY].size());
        showingSuggestions = false;
        fileModified = true;
        statusMessage = "Redo";
        updateScroll();
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
