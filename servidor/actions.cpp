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

bool check_user_exists(string username) {
    string path = "./users";

    for (const auto & file : directory_iterator(path)) {
        string name = file.path().filename().replace_extension("");
        if (name.compare(username) == 0) return true;
    }
    return false;
}

bool check_user_online(string username) {
    string path = "./online";

    for (const auto & file : directory_iterator(path)) {
        string name = file.path().filename().replace_extension("");
        if (name.compare(username) == 0) return true;
    }
    return false;
}

void add_new_user(string username, string password, string points) {
    ofstream userfile("./users/"+username+".txt");
    userfile << password << " " << points << endl;
    userfile.close();
}

void add_active_user(string username, string ip_addr, string port) {
    ofstream userfile("./online/"+username+".txt");
    userfile << ip_addr << " " << port << endl;
    userfile.close();
}

void remove_active_user(string username) {
    remove("./online/"+username+".txt");
}

string get_user_password(string username) {
    ifstream userfile("./users/"+username+".txt");
    string password, points;
    userfile >> password >> points;
    return password;
}

void set_user_password(string username, string new_password) {
    string path = "./users/"+username+".txt";
    string temp_path = "./users/"+username+"_temp.txt";
    ifstream userfile(path);
    string password, points;
    userfile >> password >> points;
    userfile.close();
    add_new_user(username+"_temp", new_password, points);
    remove(path);
    rename(temp_path, path);
}

bool sort_lideres(vector<string> x, vector<string> y) {
    if (x[1].compare(y[1]) == 0) return (x[0] < y[0]);
    return (x[1] > y[1]);
}

string get_lideres() {
    vector<vector<string>> leaders {};
    string path = "./users";
    for (const auto & file : directory_iterator(path)) {
        string username = file.path().filename().replace_extension("");
        ifstream userfile(path + "/" + username + ".txt");
        string password, points;
        userfile >> password >> points;
        userfile.close();
        vector<string> user = {username, points};
        leaders.push_back(user);
    }
    sort(leaders.begin(), leaders.end(), sort_lideres);
    string result = "leaders";
    for (vector<string> user : leaders)
        result = result + " " + user[0] + " " + user[1];
    return result;
}

string get_usuarios_ativos() {
    string ativos = "active";
    string path = "./online";
    for (const auto & file : directory_iterator(path)) {
        string username = file.path().filename().replace_extension("");
        ativos = ativos + " " + username;
    }
    return ativos;
}

void add_points(string username, int points_to_add) {
    string path = "./users/"+username+".txt";
    string temp_path = "./users/"+username+"_temp.txt";
    ifstream userfile(path);
    string password, points;
    userfile >> password >> points;
    userfile.close();
    points = to_string(stoi(points) + points_to_add);
    add_new_user(username + "_temp", password, points);
    remove("./users/"+username+".txt");
    rename(temp_path, path);
}

void register_draw(string player_name, string challenger_name) {
    add_points(player_name, 1);
    add_points(challenger_name, 1);
}

void register_win(string winner) {
    add_points(winner, 2);
}
