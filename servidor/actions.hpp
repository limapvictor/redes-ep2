#include <cstring>
using namespace std;

typedef struct net_address {
    string ip_addr;
    string port;
} net_address;

typedef net_address* net_addr;

bool check_user_exists(string username);

bool check_user_online(string username);

bool check_user_playing(string username);

void add_new_user(string username, string password, string points);

void add_active_user(string username, string ip_addr, string port);

void add_match(string player_name, string challenger_name);

void remove_match(string player_name, string challenger_name);

void remove_active_user(string username);

string get_user_password(string username);

void set_user_password(string username, string password);

net_addr get_user_net_info(string username);

string get_lideres();

string get_usuarios_ativos();

void register_draw(string player_name, string challenger_name);

void register_win(string winner);


