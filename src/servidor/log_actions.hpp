void log_init();

bool last_execution_succeeded();

void log_client_connected(string ip_addr);

void log_client_disconnected(string ip_addr);

void log_client_crash(string ip_addr);

void log_login_fail(string username, string ip_addr, string error);

void log_login_success(string username, string ip_addr);

void log_match_start(string player1, string player2, string ip1, string ip2);

void log_player_win(string winner, string loser, string ip_winner, string ip_loser);

void log_draw(string player1, string player2, string ip1, string ip2);

void log_server_stopped();