#include <sys/prctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <filesystem>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../utils.hpp"
#include "actions.hpp"
#include "log_actions.hpp"
using namespace std;

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

typedef struct usuario {
    bool logged_in;
    string name;
    string ip_address;
    string game_port;
    string hb_port;
    bool is_playing;
    string challenger_name;
    string challenger_ip_address;
    string challenger_port;
} usuario;

typedef usuario* user;

void login(user usuario, string username, string ip_addr, string hb_port, string game_port) {
    add_active_user(username, ip_addr, hb_port, game_port);
    usuario->logged_in = true;
    usuario->name = username;
    usuario->ip_address = ip_addr;
    usuario->hb_port = hb_port;
    usuario->game_port = game_port;
}

void logout(user usuario) {
    remove_active_user(usuario->name);
    usuario->logged_in = false;
}

void restore_user(user usuario, string username) {
    usuario->logged_in = true;
    usuario->name = username;

    net_addr user_info = get_user_net_info(username);
    usuario->ip_address = user_info->ip_addr;
    usuario->hb_port = user_info->hb_port;
    usuario->game_port = user_info->game_port;

    usuario->is_playing = true;
    string oponente = get_opponent(username);
    net_addr opponent_info = get_user_net_info(oponente);
    usuario->challenger_name = oponente;
    usuario->challenger_ip_address = opponent_info->ip_addr;
    usuario->challenger_port = opponent_info->game_port;
}

void server_stop(int signum) {
    cout << "\nStopping server gracefully...\n\n";
    log_server_stopped();
    std::exit(0);
}

void stop_child(int signum) {
    std::exit(0);
}

int create_hb_socket(string ip_addr, string port) {
    int sockfd;
    struct sockaddr_in hb_clientaddr;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    
    bzero(&hb_clientaddr, sizeof(hb_clientaddr));
    hb_clientaddr.sin_family = AF_INET;
    hb_clientaddr.sin_port = htons(stoi(port));

    if (inet_pton(AF_INET, ip_addr.c_str(), &hb_clientaddr.sin_addr) <= 0) {
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr *) &hb_clientaddr, sizeof(hb_clientaddr)) < 0) {
        return -1;
    }
    return sockfd;
}

int invite_player(int connfd, string ip_addr, string port) {
    int sockfd;
    struct sockaddr_in other_clientaddr;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        string error_message = "reject O convite não pode ser enviado. Tente novamente";
        write(connfd, error_message.c_str(), error_message.length());
        return -1;
    }
    
    bzero(&other_clientaddr, sizeof(other_clientaddr));
    other_clientaddr.sin_family = AF_INET;
    other_clientaddr.sin_port = htons(stoi(port));

    if (inet_pton(AF_INET, ip_addr.c_str(), &other_clientaddr.sin_addr) <= 0) {
        string error_message = "reject O convite não pode ser enviado. Tente novamente";
        write(connfd, error_message.c_str(), error_message.length());
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr *) &other_clientaddr, sizeof(other_clientaddr)) < 0) {
        string error_message = "reject O convite não pode ser enviado. Tente novamente";
        write(connfd, error_message.c_str(), error_message.length());
        return -1;
    }
    return sockfd;
}

SSL_CTX* create_server_ssl_context() {
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_ecdh_auto(ctx, 1);
    SSL_CTX_use_PrivateKey_file(ctx, "server-private-key.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_certificate_file(ctx, "server-certificate.pem", SSL_FILETYPE_PEM);
    SSL_CTX_set_cipher_list(ctx, "ECDHE-RSA-AES128-GCM-SHA256");
    return ctx;
}

vector<string> get_SSL_read(SSL_CTX* ctx, int sockfd, char* recvline) {
    SSL* ssl;
    ssl = SSL_new(ctx);
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    SSL_set_fd(ssl, sockfd);
    SSL_accept(ssl);
    int n = SSL_read(ssl, recvline, MAXLINE);
    recvline[n] = 0;
    SSL_shutdown(ssl);
    SSL_free(ssl);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    return convertAndSplit(recvline);
}

int main (int argc, char **argv) {
    
    int listenfd, connfd;
    
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_size = sizeof(struct sockaddr_in);
    
    pid_t childpid;
    
    char recvline[MAXLINE + 1] = {};
    
    ssize_t n, hb_n;

    signal(SIGINT, server_stop);

    create_server_directories();

    log_init();

    if (!last_execution_succeeded()) {
        remove_online_users();
    }

    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de jogo da velha na porta <Porta> TCP\n");
        std::exit(1);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Erro ao criar socket.\n");
        std::exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("Erro ao conectar socket à porta\n");
        std::exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("Erro ao esperar por conexões.\n");
        std::exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
	for (;;) {

        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("Erro ao aceitar conexão\n");
            std::exit(5);
        }
        
        if ( (childpid = fork()) == 0) {
            /**** PROCESSO FILHO ****/
            printf("[Uma conexão aberta]\n");
           
            close(listenfd);

            signal(SIGINT, SIG_DFL);

            prctl(PR_SET_PDEATHSIG, SIGHUP);

            signal(SIGHUP, stop_child);

            getpeername(connfd, (struct sockaddr *)&clientaddr, &clientaddr_size);
            string ip_addr = inet_ntoa(clientaddr.sin_addr);

            log_client_connected(ip_addr);

            SSL_library_init();
            SSL_CTX* ctx = create_server_ssl_context();

            string hb_port;
            string game_port;
            user current_user = new usuario;

            n = read(connfd, recvline, MAXLINE);

            vector<string> start_message = convertAndSplit(recvline);
            string comando = start_message[0];
            if (comando.compare("info") == 0) {
                hb_port = start_message[1];
                game_port = start_message[2];                
                current_user->logged_in = false;
                current_user->is_playing = false;
            }
            else if (comando.compare("reconnect") == 0) {
                string usuario = start_message[1];
                restore_user(current_user, usuario);
                hb_port = current_user->hb_port;
                game_port = current_user->game_port;
            }
         
            write(connfd, "success", 7);

            fcntl(connfd, F_SETFL, O_NONBLOCK);

            clock_t heartbeat_send_time = clock();
            bool heartbeat_sent = false;

            clock_t accept_time;

            int hb_fd;

            for (;;) {

                if (heartbeat_sent && ((float) ((clock() - accept_time) / CLOCKS_PER_SEC) > 180)) {
                    if (current_user->logged_in) logout(current_user);
                    log_client_crash(ip_addr);
                    break;
                }
                    
                n = read(connfd, recvline, MAXLINE);

                if (n == -1 && errno == EAGAIN && !heartbeat_sent) {
                    if ((float) ((clock() - heartbeat_send_time) / CLOCKS_PER_SEC) >= 50) {
                        hb_fd = create_hb_socket(ip_addr, hb_port);
                        fcntl(hb_fd, F_SETFL, O_NONBLOCK);
                        write(hb_fd, "heartbeat", 9);
                        heartbeat_send_time = clock();
                        heartbeat_sent = true;
                        accept_time = clock();
                    }
                }

                if (heartbeat_sent) {
                    hb_n = read(hb_fd, recvline, MAXLINE);
                    if (hb_n == -1 && errno == EAGAIN);
                    else if (hb_n > 0) {
                        heartbeat_sent = false;     
                        close(hb_fd);           
                    }
                } 

                if (n < 0) {
                    continue;
                }

                recvline[n] = 0;

                vector<string> mensagem = convertAndSplit(recvline);
                string comando = mensagem[0];
                if (comando.compare("encrypted") == 0) {
                    mensagem = get_SSL_read(ctx, connfd, recvline);
                    comando = mensagem[0];
                }

                if (comando.compare("adduser") == 0) {

                    if (current_user->logged_in) {
                        string error_message = "error Você já está logado";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    string username = mensagem[1];
                    string password = mensagem[2];
                    bool username_exists = check_user_exists(username);

                    if (username_exists) {
                        string error_message = "error Esse usuário já existe";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    add_new_user(username, password, "0");
                    login(current_user, username, ip_addr, hb_port, game_port);
                    write(connfd, "success", 7);
                    log_login_success(username, ip_addr);
                }
                else if (comando.compare("passwd") == 0) {

                    if (!(current_user->logged_in)) {
                        string error_message = "error Você deve estar logado para usar esse comando";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    string old_password = mensagem[1];
                    string new_password = mensagem[2];
                    string current_password = get_user_password(current_user->name);

                    if (old_password.compare(current_password) == 0) {
                        set_user_password(current_user->name, new_password);
                        write(connfd, "success", 7);
                        continue;
                    }

                    string error_message = "error A senha atual está incorreta";
                    write(connfd, error_message.c_str(), error_message.length());
                }
                else if (comando.compare("login") == 0) {

                    string username = mensagem[1];

                    if (current_user->logged_in) {
                        string error_message = "error Você já está logado";
                        write(connfd, error_message.c_str(), error_message.length());
                        log_login_fail(username, ip_addr, "Usuário já estava logado.\n");
                        continue;
                    }

                    bool has_account = check_user_exists(username);

                    if (!has_account) {
                        string error_message = "error Esse usuário não está cadastrado";
                        write(connfd, error_message.c_str(), error_message.length());
                        log_login_fail(username, ip_addr, "Usuário não está registrado.\n");
                        continue;
                    }

                    bool already_logged = check_user_online(username);

                    if (already_logged) {
                        string error_message = "error Esse usuário já está logado de outro lugar";
                        write(connfd, error_message.c_str(), error_message.length());
                        log_login_fail(username, ip_addr, "Usuário já estava logado em outro lugar.\n");
                        continue;
                    }

                    string password = mensagem[2];
                    string current_password = get_user_password(username);

                    if (password.compare(current_password) == 0) {
                        login(current_user, username, ip_addr, hb_port, game_port);
                        write(connfd, "success", 7);
                        log_login_success(username, ip_addr);
                        continue;
                    }

                    string error_message = "error A senha está incorreta";
                    write(connfd, error_message.c_str(), error_message.length());
                    log_login_fail(username, ip_addr, "Senha incorreta.\n");
                }
                else if (comando.compare("leaders") == 0) {
                    string lideres = get_lideres();
                    write(connfd, lideres.c_str(), lideres.length());
                }
                else if (comando.compare("list") == 0) {

                    if (!(current_user->logged_in)) {
                        string error_message = "error Você precisa estar logado para ver os jogadores ativos";
                        write(connfd, error_message.c_str(), error_message.length());
                    }
                    else {
                        string ativos = get_usuarios_ativos();
                        write(connfd, ativos.c_str(), ativos.length());
                    }
                }
                else if (comando.compare("begin") == 0) {

                    if (!(current_user->logged_in)) {
                        string error_message = "error Você deve estar logado para iniciar uma partida";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    string oponente = mensagem[1];
                    bool exists = check_user_exists(oponente);
                    if (!exists) {
                        string error_message = "error Esse usuário não existe";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    bool online = check_user_online(oponente);
                    if (!online) {
                        string error_message = "error Esse jogador não está online agora";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    bool is_playing = check_user_playing(oponente);
                    if (is_playing) {
                        string error_message = "error Esse jogador já está em uma partida";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    net_addr client_info = get_user_net_info(oponente);

                    int sockfd = invite_player(connfd, client_info->ip_addr, client_info->game_port);

                    string invite = "invite " + current_user->name;
                    write(sockfd, invite.c_str(), invite.length());

                    n = read(sockfd, recvline, MAXLINE);
                    recvline[n] = '\0';

                    vector<string> answer_message = convertAndSplit(recvline);
                    string answer = answer_message[0];

                    if (answer.compare("reject") == 0) {
                        string error_message = "reject O jogador recusou o convite";
                        write(connfd, error_message.c_str(), error_message.length());
                    }
                    else if (answer.compare("accept") == 0) {
                        current_user->challenger_name = oponente;
                        current_user->challenger_ip_address = client_info->ip_addr;
                        current_user->challenger_port = client_info->game_port;
                        current_user->is_playing = true;
                        string player_symbol, opponent_symbol;
                        if ((rand() % 2) == 0) {
                            player_symbol = "X";
                            opponent_symbol = "O";
                        } else {
                            player_symbol = "O";
                            opponent_symbol = "X";
                        }
                        string response = "accept " + client_info->ip_addr + " " + client_info->game_port + " " + player_symbol;
                        if ((rand() % 2) == 0) {
                            response = response + " " + opponent_symbol;
                        } else {
                            response = response + " " + player_symbol;
                        }
                        write(connfd, response.c_str(), response.length());
                        add_match(current_user->name, oponente);
                        log_match_start(current_user->name, oponente, current_user->ip_address, current_user->challenger_ip_address);
                    }
                    else {
                        string error_message = "reject O convite não pode ser enviado. Tente novamente";
                        write(connfd, error_message.c_str(), error_message.length());
                    }
                }
                else if (comando.compare("logout") ==0) {

                    if (!(current_user->logged_in)) {
                        string error_message = "error Você não está logado";
                        write(connfd, error_message.c_str(), error_message.length());
                        continue;
                    }

                    logout(current_user);
                    write(connfd, "success", 7);
                }
                else if (comando.compare("exit") == 0) {

                    if (current_user->logged_in) logout(current_user);
                    write(connfd, "success", 7);
                    break;
                }
                else if (comando.compare("result") == 0) {

                    string status = mensagem[1];

                    if (status.compare("draw") == 0) {
                        register_draw(current_user->name, current_user->challenger_name);
                        log_draw(current_user->name, current_user->challenger_name, current_user->ip_address, current_user->challenger_ip_address);
                    }
                    else if (status.compare("victory") == 0) {
                        register_win(current_user->name);
                        log_player_win(current_user->name, current_user->challenger_name, current_user->ip_address, current_user->challenger_ip_address);
                    }

                    current_user->is_playing = false;
                    write(connfd, "success", 7);
                    remove_match(current_user->name, current_user->challenger_name);
                }
                else if (comando.compare("endgame") == 0) {

                    current_user->is_playing = false;
                    write(connfd, "success", 7);
                }
                else if (comando.compare("startgame") == 0) {
                    string oponente = mensagem[1];

                    net_addr challenger_info = get_user_net_info(oponente);
                    current_user->challenger_name = oponente;
                    current_user->challenger_ip_address = challenger_info->ip_addr;
                    current_user->challenger_port = challenger_info->game_port;
                    current_user->is_playing = true;
                }
            }

            log_client_disconnected(ip_addr);

            printf("[Uma conexão fechada]\n");
            close(connfd);
            std::exit(0);
        }
        else
            /**** PROCESSO PAI ****/
            close(connfd);
    }
    std::exit(0);
}