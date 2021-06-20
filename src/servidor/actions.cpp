#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include "actions.hpp"
using namespace std;
using filesystem::directory_iterator;
using filesystem::remove;
using filesystem::rename;
using filesystem::exists;

#define USERS_PATH "./users/"
#define ONLINE_PATH "./online/"
#define MATCHES_PATH "./matches/"

bool check_user_exists(string username) {
    return exists(USERS_PATH + username);
}

bool check_user_online(string username) {
    return exists(ONLINE_PATH + username);
}

bool check_user_playing(string username) {
    return exists(MATCHES_PATH + username);
}

void add_new_user(string username, string password, string points) {
    ofstream userfile(USERS_PATH + username);
    userfile << password << " " << points;
    userfile.close();
}

void add_active_user(string username, string ip_addr, string port) {
    ofstream userfile(ONLINE_PATH + username);
    userfile << ip_addr << " " << port;
    userfile.close();
}

void add_match(string player_name, string challenger_name) {
    ofstream player_file(MATCHES_PATH + player_name);
    player_file << challenger_name;
    player_file.close();

    ofstream challenger_file(MATCHES_PATH + challenger_name);
    challenger_file << player_name;
    challenger_file.close();
}

void remove_match(string player_name, string challenger_name) {
    remove(MATCHES_PATH + player_name);
    remove(MATCHES_PATH + challenger_name);
}

void remove_active_user(string username) {
    remove(ONLINE_PATH + username);
}

string get_user_password(string username) {
    ifstream userfile(USERS_PATH + username);
    string password, points;
    userfile >> password >> points;
    return password;
}

void set_user_password(string username, string new_password) {
    ifstream userfile(USERS_PATH + username);
    string password, points;
    userfile >> password >> points;
    userfile.close();
    add_new_user(username, new_password, points);
}

net_addr get_user_net_info(string username) {
    ifstream userfile(ONLINE_PATH + username);
    string ip_addr, port;
    userfile >> ip_addr >> port;
    net_addr info = new net_address;
    info->ip_addr = ip_addr;
    info->port = port;
    return info;
}

bool sort_lideres(vector<string> x, vector<string> y) {
    if (x[1].compare(y[1]) == 0) return (x[0] < y[0]);
    return (x[1] > y[1]);
}

string get_lideres() {
    vector<vector<string>> leaders {};
    for (const auto & file : directory_iterator("./users")) {
        string username = file.path().filename().replace_extension("");
        ifstream userfile(USERS_PATH + username);
        string password, points;
        userfile >> password >> points;
        userfile.close();
        vector<string> user = {username, points};
        leaders.push_back(user);
    }
    sort(leaders.begin(), leaders.end(), sort_lideres);
    string result = "leaders";
    for (vector<string> user : leaders)
        result = result + "\n" + user[0] + " " + user[1];
    return result;
}

string get_usuarios_ativos() {
    string ativos = "active";
    for (const auto & file : directory_iterator("./online")) {
        string username = file.path().filename().replace_extension("");
        string condition;
        if (check_user_playing(username)) condition = " -> In a match";
        else condition = " -> Not playing";
        ativos = ativos + '\n' + username + condition;
    }
    return ativos;
}

void add_points(string username, int points_to_add) {
    ifstream userfile(USERS_PATH + username);
    string password, points;
    userfile >> password >> points;
    userfile.close();
    points = to_string(stoi(points) + points_to_add);
    add_new_user(username, password, points);
}

void register_draw(string player_name, string challenger_name) {
    add_points(player_name, 1);
    add_points(challenger_name, 1);
}

void register_win(string winner) {
    add_points(winner, 2);
}
