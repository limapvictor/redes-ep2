#include "utils.hpp"

vector<string> convertAndSplit(char array[]) {

    string converted(array);
    string sep = " ";

    converted.erase(converted.find_last_not_of(" \n\r\t")+1);
    vector<string> splitted {};

    size_t pos = 0;
    string token;
    while ((pos = converted.find(sep)) != string::npos) {
        token = converted.substr(0, pos);
        splitted.push_back(token);
        converted.erase(0, pos + sep.length());
    }
    token = converted.substr(0);
    splitted.push_back(token);
    return splitted;
}
