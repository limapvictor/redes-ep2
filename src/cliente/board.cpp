#include <iostream>
#include <vector>
#include <math.h>

#include "./board.hpp"

std::vector<string> symbols {" ", "X", "O"};

int board;

string inverseSymbol(string symbol) {
    return symbol == "X" ? "O" : "X";
}

string getSymbolFromInt(int symbolAsInt) {
    return symbols[symbolAsInt];
}

int getIntFromPlayerSymbol(string symbol) {
    if (symbol == " ") return 0;
    return symbol == "X" ? 1 : 2;
}

string getPlayerInPosition(int line, int column) {
    int player;

    player = fmod(board / pow(3, 3 * line + column), 3);
    return getSymbolFromInt(player);
}

int isSequenceComplete(int x, int y, int z) {
    if (x == y && y == z) return x;
    return 0;
}

int getGameCurrentResult() {
    int posA, posB, posC, posD, posE, posF, posG, posH, posI;
    int lines, columns, diagonal1, diagonal2;

    posA = fmod(board / pow(3, 0), 3);
    posB = fmod(board / pow(3, 1), 3);
    posC = fmod(board / pow(3, 2), 3);
    posD = fmod(board / pow(3, 3), 3);
    posE = fmod(board / pow(3, 4), 3);
    posF = fmod(board / pow(3, 5), 3);
    posG = fmod(board / pow(3, 6), 3);
    posH = fmod(board / pow(3, 7), 3);
    posI = fmod(board / pow(3, 8), 3);

    lines = isSequenceComplete(posA, posB, posC) + isSequenceComplete(posD, posE, posF) + isSequenceComplete(posG, posH, posI);
    if (lines > 0) return lines;
    columns = isSequenceComplete(posA, posD, posG) + isSequenceComplete(posB, posE, posH) + isSequenceComplete(posC, posF, posI);
    if (columns > 0) return columns;
    diagonal1 = isSequenceComplete(posA, posE, posI);
    if (diagonal1 > 0) return diagonal1;
    diagonal2 = isSequenceComplete(posC, posE, posG);
    if (diagonal2 > 0) return diagonal2;

    if (posA > 0 && posB > 0 && posC > 0 && posD > 0 && posE > 0 && posF > 0 && posG > 0 && posH > 0 && posI > 0) return 0;
    return -1;
}

void resetBoard() {
    board = 0;
}

void updateBoard(string player, int line, int column) {
    board += pow(3, (3 * (line - 1) + (column - 1))) * (getIntFromPlayerSymbol(player));
    std::cout << "Tabuleiro atualizado: " << std::endl;
}

void printBoard() {
    std::cout << " " << getPlayerInPosition(0, 0) << " " << "|" << " " << getPlayerInPosition(0, 1) << " " << "|" << " " << getPlayerInPosition(0, 2) <<std::endl;
    std::cout << "---+---+---" << std::endl;
    std::cout << " " << getPlayerInPosition(1, 0) << " " << "|" << " " << getPlayerInPosition(1, 1) << " " << "|" << " " << getPlayerInPosition(1, 2) <<std::endl;
    std::cout << "---+---+---" << std::endl;
    std::cout << " " << getPlayerInPosition(2, 0) << " " << "|" << " " << getPlayerInPosition(2, 1) << " " << "|" << " " << getPlayerInPosition(2, 2) <<std::endl;
}