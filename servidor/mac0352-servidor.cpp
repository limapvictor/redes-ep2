#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <ctime>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <iostream>
#include <signal.h>
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
    string port;
    bool is_playing;
    string challenger_name;
} usuario;

typedef usuario* user;

void login(user usuario, string username, string ip_addr, string port) {
    add_active_user(username, ip_addr, port);
    usuario->logged_in = true;
    usuario->name = username;
    usuario->ip_address = ip_addr;
    usuario->port = port;
}

void logout(user usuario) {
    remove_active_user(usuario->name);
    usuario->logged_in = false;
}

void server_stop(int signum) {
    cout << "\nStopping server gracefully...\n\n";
    log_server_stopped();
    std::exit(0);
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

int main (int argc, char **argv) {
    /* Os sockets. Um que será o socket que vai escutar pelas conexões
     * e o outro que vai ser o socket específico de cada conexão */
    int listenfd, connfd;
    /* Informações sobre o socket (endereço e porta) ficam nesta struct */
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;
    /* Retorno da função fork para saber quem é o processo filho e
     * quem é o processo pai */
    pid_t childpid;
    /* Armazena linhas recebidas do cliente */
    char recvline[MAXLINE + 1] = {};
    /* Armazena o tamanho da string lida do cliente */
    ssize_t n;

    signal(SIGINT, server_stop);

    log_init();

    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de jogo da velha na porta <Porta> TCP\n");
        std::exit(1);
    }

    /* Criação de um socket. É como se fosse um descritor de arquivo.
     * É possível fazer operações como read, write e close. Neste caso o
     * socket criado é um socket IPv4 (por causa do AF_INET), que vai
     * usar TCP (por causa do SOCK_STREAM), já que o MQTT funciona sobre
     * TCP, e será usado para uma aplicação convencional sobre a Internet
     * (por causa do número 0) */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        std::exit(2);
    }

    /* Agora é necessário informar os endereços associados a este
     * socket. É necessário informar o endereço / interface e a porta,
     * pois mais adiante o socket ficará esperando conexões nesta porta
     * e neste(s) endereços. Para isso é necessário preencher a struct
     * servaddr. É necessário colocar lá o tipo de socket (No nosso
     * caso AF_INET porque é IPv4), em qual endereço / interface serão
     * esperadas conexões (Neste caso em qualquer uma -- INADDR_ANY) e
     * qual a porta. Neste caso será a porta que foi passada como
     * argumento no shell (atoi(argv[1]))
     */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        std::exit(3);
    }

    /* Como este código é o código de um servidor, o socket será um
     * socket passivo. Para isto é necessário chamar a função listen
     * que define que este é um socket de servidor que ficará esperando
     * por conexões nos endereços definidos na função bind. */
    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        std::exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
    /* O servidor no final das contas é um loop infinito de espera por
     * conexões e processamento de cada uma individualmente */
    bzero(&clientaddr, sizeof(clientaddr));
	for (;;) {
        /* O socket inicial que foi criado é o socket que vai aguardar
         * pela conexão na porta especificada. Mas pode ser que existam
         * diversos clientes conectando no servidor. Por isso deve-se
         * utilizar a função accept. Esta função vai retirar uma conexão
         * da fila de conexões que foram aceitas no socket listenfd e
         * vai criar um socket específico para esta conexão. O descritor
         * deste novo socket é o retorno da função accept. */
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            std::exit(5);
        }
        /* Agora o servidor precisa tratar este cliente de forma
         * separada. Para isto é criado um processo filho usando a
         * função fork. O processo vai ser uma cópia deste. Depois da
         * função fork, os dois processos (pai e filho) estarão no mesmo
         * ponto do código, mas cada um terá um PID diferente. Assim é
         * possível diferenciar o que cada processo terá que fazer. O
         * filho tem que processar a requisição do cliente. O pai tem
         * que voltar no loop para continuar aceitando novas conexões.
         * Se o retorno da função fork for zero, é porque está no
         * processo filho. */
        if ( (childpid = fork()) == 0) {
            /**** PROCESSO FILHO ****/
            printf("[Uma conexão aberta]\n");
            /* Já que está no processo filho, não precisa mais do socket
             * listenfd. Só o processo pai precisa deste socket. */
            close(listenfd);

            //string ip_addr = inet_ntoa(clientaddr.sin_addr);
            string ip_addr = "127.0.0.1";

            log_client_connected(ip_addr);

            n = read(connfd, recvline, MAXLINE);

            vector<string> info_message = convertAndSplit(recvline);
            while (info_message[0].compare("info") != 0) {
                write(connfd, "error", 5);
                n = read(connfd, recvline, MAXLINE);
                info_message = convertAndSplit(recvline);
            }

            write(connfd, "success", 7);
            string port = info_message[1];
         
            /* Agora pode ler do socket e escrever no socket. Isto tem
             * que ser feito em sincronia com o cliente. Não faz sentido
             * ler sem ter o que ler. Ou seja, neste caso está sendo
             * considerado que o cliente vai enviar algo para o servidor.
             * O servidor vai processar o que tiver sido enviado e vai
             * enviar uma resposta para o cliente (Que precisará estar
             * esperando por esta resposta) 
             */

            user current_user = new usuario;
            current_user->logged_in = false;
            current_user->is_playing = false;
            
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */
            /* TODO: É esta parte do código que terá que ser modificada
             * para que este servidor consiga interpretar comandos MQTT  */

            //HEARTBEATfcntl(connfd, F_SETFL, O_NONBLOCK);

            //HEARTBEATclock_t heartbeat_send_time = clock();
            //HEARTBEATbool heartbeat_sent = false;
            //HEARTBEATbool heartbeat_received = false;

            //HEARTBEATclock_t accept_time;

            for (;;) {

                // if (heartbeat_sent && ((float) ((clock() - accept_time) / CLOCKS_PER_SEC) > 180)) {
                //     handleClientCrash();
                // }
                    
                n = read(connfd, recvline, MAXLINE);
                recvline[n] = 0;

                // HEARTBEATif (n == -1 && errno == EAGAIN) {
                //     if ((float) ((clock() - heartbeat_send_time) / CLOCKS_PER_SEC) >= 50) {
                //         write(connfd, "heartbeat", 9);
                //         heartbeat_send_time = clock();
                //         heartbeat_sent = true;
                //         accept_time = clock();
                //     }
                //     continue;
                // }

                if (n < 0) {
                    cout << "Error reading packet";
                    continue;
                }

                vector<string> mensagem = convertAndSplit(recvline);
                string comando = mensagem[0];

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
                    login(current_user, username, ip_addr, port);
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
                        login(current_user, username, ip_addr, port);
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
                // else if (comando.compare("begin") == 0) {

                //     if (!(current_user->logged_in)) {
                //         string error_message = "error Você deve estar logado para iniciar uma partida";
                //         write(connfd, error_message.c_str(), error_message.length());
                //         continue;
                //     }

                //     string oponente = mensagem[1];
                //     bool exists = check_user_exists(oponente);
                //     if (!exists) {
                //         string error_message = "error Esse usuário não existe";
                //         write(connfd, error_message.c_str(), error_message.length());
                //         continue;
                //     }

                //     bool online = check_user_online(oponente);
                //     if (!online) {
                //         string error_message = "error Esse jogador não está online agora";
                //         write(connfd, error_message.c_str(), error_message.length());
                //         continue;
                //     }

                //     bool is_playing = check_user_playing(oponente);
                //     if (is_playing) {
                //         string error_message = "error Esse jogador já está em uma partida";
                //         write(connfd, error_message.c_str(), error_message.length());
                //         continue;
                //     }

                //     net_addr client_info = get_user_net_info(oponente);

                //     int sockfd = invite_player(connfd, client_info->ip_addr, client_info->port);

                //     string invite = "invite " + current_user->name;
                //     write(sockfd, invite.c_str(), invite.length());

                //     n = read(sockfd, recvline, MAXLINE);

                //     vector<string> answer_message = convertAndSplit(recvline);
                //     string answer = answer_message[0];

                //     if (answer.compare("reject") == 0) {
                //         string error_message = "reject O jogador recusou o convite";
                //         write(connfd, error_message.c_str(), error_message.length());
                //     }
                //     else if (answer.compare("accept") == 0) {
                //         current_user->challenger_name = oponente;
                //         current_user->is_playing = true;
                //         string player_symbol, opponent_symbol;
                //         if ((rand() % 2) == 0) {
                //             player_symbol = "X";
                //             opponent_symbol = "O";
                //         } else {
                //             player_symbol = "O";
                //             opponent_symbol = "X";
                //         }
                //         string response = "accept " + client_info->ip_addr + " " + client_info->port + " ";
                //         if ((rand() % 2) == 0) {
                //             response = response + current_user->name + " " + player_symbol + " " + oponente + " " + opponent_symbol;
                //         } else {
                //             response = response + oponente + " " + opponent_symbol + " " + current_user->name + " " + player_symbol;
                //         }
                //         write(connfd, response.c_str(), response.length());
                //         add_match(current_user->name, oponente);
                //     }
                //     else {
                //         string error_message = "reject O convite não pode ser enviado. Tente novamente";
                //         write(connfd, error_message.c_str(), error_message.length());
                //     }
                // }
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

                    logout(current_user);
                    write(connfd, "success", 7);
                    close(connfd);
                    break;
                }
                else if (comando.compare("result") == 0) {

                    string status = mensagem[1];

                    if (status.compare("draw") == 0) {
                        string player = mensagem[2];
                        string oponente = mensagem[3];
                        register_draw(player, oponente);
                    }
                    else if (status.compare("victory") == 0) {
                        string winner = mensagem[2];
                        register_win(winner);
                    }

                    current_user->is_playing = false;
                    write(connfd, "success", 7);
                }
                else if (comando.compare("endgame")) {

                    current_user->is_playing = false;
                    current_user->challenger_name = "";
                    write(connfd, "success", 7);
                }

                // HEARTBEATif (heartbeat_sent) {
                //         n = read(connfd, recvline, MAXLINE);
                //         if (n == -1 && errno == EAGAIN);
                //         else if (n >= 0) {
                //             heartbeat_sent = false;
                //             heartbeat_received = true;                   
                //         }
                //     }
                // }     
            }
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */

            log_client_disconnected(ip_addr);
            /* Após ter feito toda a troca de informação com o cliente,
             * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            std::exit(0);
        }
        else
            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            close(connfd);
    }
    std::exit(0);
}