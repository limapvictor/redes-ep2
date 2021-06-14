#include <cstring>
#include <vector>
#include <iostream>
#include <map>

using namespace std;

vector<string> convertAndSplit(char array[]);

std::map<string, int> COMMAND_TYPES {
    {"adduser", 0},
    {"passwd", 1},
    {"login", 2},
    {"leaders", 3},
    {"list", 4},
    {"begin", 5},
    {"send", 6},
    {"delay", 7},
    {"end", 8},
    {"logout", 9},
    {"exit", 10}
};