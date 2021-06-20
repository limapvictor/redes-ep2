#include <string>

using namespace std;

string inverseSymbol(string symbol);

string getSymbolFromInt(int symbolAsInt);

int getIntFromPlayerSymbol(string symbol);

string getPlayerInPosition(int line, int column);

int isSequenceComplete(int x, int y, int z);

int getGameCurrentResult();

void resetBoard();

void updateBoard(string player, int line, int column);

void printBoard();