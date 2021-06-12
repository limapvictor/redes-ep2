#include <cstring>
using namespace std;

bool check_user_exists(string username);

bool check_user_online(string username);

void add_new_user(string username, string password);

void add_active_user(string username, string ip_addr);

string get_user_password(string username);

void set_user_password(string username);

string get_lideres();

string get_usuarios_ativos();

void register_draw(string player_name, string challenger_name);

void register_win(string winner);


