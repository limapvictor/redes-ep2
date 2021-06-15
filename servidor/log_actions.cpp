#include <string>
using namespace std;
#include "log_actions.hpp"

void log_init(){}

void log_client_connected(string ip_addr){}

void log_client_disconnected(string ip_addr){}

void log_login_fail(string username, string ip_addr, string error){}

void log_login_success(string username, string ip_addr){}

void log_server_stopped(){}