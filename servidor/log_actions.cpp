#include <filesystem>
#include <fstream>
using namespace std;
#include "log_actions.hpp"
using filesystem::exists;

#define LOGPATH "./log/log"

bool check_last_log_line_success() {
    return true;
}

void log_init(){

    bool log_exists = exists(LOGPATH);
        
    ofstream logfile(LOGPATH, ios_base::app);

    if (!log_exists) {
        logfile << "Servidor iniciado com sucesso.\n";
        logfile.close();
        return;
    }

    logfile << "\nServidor iniciado com sucesso. Última execução foi finalizada ";

    bool graceful = check_last_log_line_success();

    if (graceful) {
        logfile << "de maneira correta.\n";
        logfile.close();
        return;
    }
    logfile << "com falhas.\nRestaurando heartbeats com os clientes.\n";
    logfile.close();
    return;
}

void log_client_connected(string ip_addr){

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Cliente conectado (IP " + ip_addr + ")\n";
}

void log_client_disconnected(string ip_addr){

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Cliente desconectado (IP " + ip_addr + ")\n";
}

void log_client_crash(string ip_addr) {
    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Conexão perdida com cliente no IP " + ip_addr + ". Heartbeat não recebeu retorno após 3 minutos.\n";
}

void log_login_fail(string username, string ip_addr, string error){

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Tentativa de login por " + username + " (IP " + ip_addr + ") falhou: " + error;
}

void log_login_success(string username, string ip_addr){
    
    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Usuário " + username + " logado com sucesso. (IP " + ip_addr + ")\n";
}

void log_player_win(string winner, string loser, string ip_winner, string ip_loser) {

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Usuário " + winner + " (IP " + ip_winner + ") derrotou usuário " + loser + " (IP " + ip_loser + ").\n";
}

void log_draw(string player1, string player2, string ip1, string ip2) {

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "A partida entre o usuário " + player1 + " (IP " + ip1 + ") e o usuário " + player2 + " (IP " + ip2 + ") terminou em empate.\n";
}

void log_server_stopped(){

    ofstream logfile(LOGPATH, ios_base::app);
    logfile << "Servidor finalizado com sucesso.";
}