#define _GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
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
#include "../utils.hpp"
#include "actions.hpp"
using namespace std;

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

typedef struct user {
    bool logged_in;
    string name;
    string ip_address;
    string port;
    bool is_playing;
    string challenger_name;
} usuario;

typedef usuario* user;

void login(user usuario, string username, string ip_addr, string port) {
    usuario->logged_in = true;
    usuario->name = username;
    usuario->ip_address = ip_addr;
    usuario->port = port;
}

void logout(user usuario) {
    usuario = (user) malloc(sizeof(usuario));
    usuario->logged_in = false;
}

int main (int argc, char **argv) {
    /* Os sockets. Um que será o socket que vai escutar pelas conexões
     * e o outro que vai ser o socket específico de cada conexão */
    int listenfd, connfd;
    /* Informações sobre o socket (endereço e porta) ficam nesta struct */
    struct sockaddr_in servaddr;
    /* Retorno da função fork para saber quem é o processo filho e
     * quem é o processo pai */
    pid_t childpid;
    /* Armazena linhas recebidas do cliente */
    char recvline[MAXLINE + 1];
    /* Armazena o tamanho da string lida do cliente */
    ssize_t n;

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
         
            /* Agora pode ler do socket e escrever no socket. Isto tem
             * que ser feito em sincronia com o cliente. Não faz sentido
             * ler sem ter o que ler. Ou seja, neste caso está sendo
             * considerado que o cliente vai enviar algo para o servidor.
             * O servidor vai processar o que tiver sido enviado e vai
             * enviar uma resposta para o cliente (Que precisará estar
             * esperando por esta resposta) 
             */

            user current_user = (user) malloc(sizeof(usuario));
            current_user->logged_in = false;
            current_user->is_playing = false;
            
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */
            /* TODO: É esta parte do código que terá que ser modificada
             * para que este servidor consiga interpretar comandos MQTT  */
            for (;;) {
                n = read(connfd, recvline, MAXLINE);
                recvline[n] = 0;

                if (n < 0) {
                    cout << "Error reading packet";
                    continue;
                }

                vector<string> mensagem = convertAndSplit(recvline);
                string comando = mensagem[0];

                if (comando.compare("adduser") == 0) {
                    if (current_user->logged_in) {
                        //enviaErro("Você já está logado.")
                    } else {
                        string username = mensagem[1];
                        string password = mensagem[2];
                        bool username_exists = check_user_exists(username);
                        if (username_exists) {
                            //enviaErro("Esse nome de usuário já existe")
                        } else {
                            //coloca usuário na tabela de usuários (username, ip, e port)
                            add_new_user(username, password);
                            login(current_user, username, ip_addr, port);
                            add_active_user(username, ip_addr, port);
                            //enviaSucesso()
                        }
                    }
                } else if (comando.compare("password") == 0) {
                    if (!(current_user->logged_in)) {
                        //enviaErro("Você deve estar logado para utilizar este comando")
                    } else {
                        string old_password = mensagem[1];
                        string new_password = mensagem[2];
                        string current_password = get_user_password(current_user->name);
                        if (old_password.compare(current_password) == 0) {
                            set_user_password(current_user->name);
                            //enviaSucesso
                        } else {
                            //enviaErro("A senha antiga digitada está incorreta")
                        }                        
                    }
                } else if (comando.compare("login") == 0) {
                    if (current_user->logged_in) {
                        //enviaErro("Você já está logado");
                    } else {
                        string username = mensagem[1];
                        string password = mensagem[2];
                        bool has_account = check_user_exists(username);
                        if (!has_account) {
                            //enviaErro("Este usuário não está cadastrado"); 
                        }
                        string current_password = get_user_password(username);
                        if (password.compare(current_password) == 0) {
                            login(current_user, username, ip_addr, port);
                            //enviaSucesso
                        } else {
                            //enviaErro("A senha informada está incorreta")
                        }
                    }
                } else if (comando.compare("leaders") == 0) {
                    string lideres = get_lideres();
                    //enviaSucesso(lideres)
                } else if (comando.compare("list") == 0) {
                    if (!(current_user->logged_in)) {
                        //enviaErro("Você precisa estar logado para ver os jogadores ativos")
                    } else {
                        string ativos = get_usuarios_ativos();
                        //enviaSucesso(ativos);
                    }
                } else if (comando.compare("begin") == 0) {
                    if (!(current_user->logged_in)) {
                        //enviaErro("Você deve estar logado para iniciar uma partida");
                    } else {
                        string oponente = mensagem[1];
                        bool exists = check_user_exists(oponente);
                        if (!exists) {
                            //enviaErro("Esse usuário não existe")
                        } else {
                            bool online = check_user_online(oponente);
                            if (!online) {
                                //enviaErro("Esse jogador não está online agora")
                            } else {
                                //enviaConvite(oponente);
                                //se oponente recusou {
                                    //enviaErro("O jogador recusou o desafio.")
                                //} else {
                                    //sorteia_inicio()
                                    current_user->challenger_name = oponente;
                                    current_user->is_playing = true;
                                    //enviaSucesso(ipoponente)
                                //}
                            }
                        }
                    }
                } else if (comando.compare("logout") ==0) {
                    if (!(current_user->logged_in)) {
                        //enviaErro("Você não está logado")
                    } else {
                        remove_active_user(current_user->name);
                        logout(current_user);
                        //enviaSucesso()
                    }
                } else if (comando.compare("exit") == 0) {
                    //enviaSucesso()
                    close(connfd);
                    break;
                } else if (comando.compare("result") == 0) {
                    string status = mensagem[1];
                    if (status.compare("draw") == 0) {
                        register_draw(current_user->name, current_user->challenger_name);
                    } else if (status.compare("victory")) {
                        string winner = mensagem[2];
                        register_win(winner);
                    }
                    current_user->is_playing = false;
                    current_user->challenger_name = "";
                    //enviaSucesso
                } else if (comando.compare("endgame")) {
                    current_user->is_playing = false;
                    current_user->challenger_name = "";
                    //enviaSucesso   
                }            
            }
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */

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