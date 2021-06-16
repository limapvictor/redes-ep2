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
#include <string>
#include <map>
#include <regex>

#include "../utils.hpp"

using namespace std;

#define MAXLINE 100
#define LISTENQ 1

int clientServerFD;
int p2pFD;

bool isClientConnected = false;
bool isUserLoggedIn = false;

string PROMPT = "JogoDaVelha> ";

void establishServerConnection(int argc, char **argv) {
    struct sockaddr_in serverAddress;
   
    if (argc != 3) {
        fprintf(stderr, "O cliente do jogo da velha deve ser chamado com: %s <SERVER IP ADDRESS> <SERVER PORT>\n", argv[0]);
        std::exit(1);
    }
   
    if ( (clientServerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr,"Erro interno de rede. Tente novamente mais tarde.\n");
        std::exit(2);
    }
   
   bzero(&serverAddress, sizeof(serverAddress));
   serverAddress.sin_family = AF_INET;
   serverAddress.sin_port = htons(std::stoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) <= 0) {
        fprintf(stderr,"Erro interno de rede. Tente novamente mais tarde.\n");
        std::exit(7);
    }
   
    if (connect(clientServerFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr,"Não foi possível se conectar com o servidor. Tente novamente mais tarde,\n");
        std::exit(3);
    }

    isClientConnected = true;
    std::cout << "-------------------------JV's-------------------------" << std::endl;
}

void initP2PListener() {
    struct sockaddr_in clientAddress;

    if ( (p2pFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(4);
    }
    fcntl(p2pFD, F_SETFL, O_NONBLOCK);

    bzero(&clientAddress, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddress.sin_port = htons(0);

    if (bind(p2pFD, (struct sockaddr *) &clientAddress, sizeof(clientAddress)) < 0 ) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(5);
    }

    if (listen(p2pFD, LISTENQ) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(6);
    }
}

string getServerResponse() {
    char buffer[MAXLINE + 1];
    int bufferSize;

    if ( (bufferSize = read(clientServerFD, buffer, MAXLINE)) < 0) {
        std::cerr << "Erro ao ler a resposta do servidor. Tente novamente." << std::endl;
        return std::string();
    }

    buffer[bufferSize] = '\0';
    return std::string(buffer);
}

bool wasRequestSuccessful() {
    string requestResult, response;
    int resultDelimiter;
    
    response = getServerResponse();
    if (response.length() == 0) return false;

    resultDelimiter = response.find(' ');
    requestResult = resultDelimiter > 0 
        ? response.substr(0, resultDelimiter) 
        : response;

    if (requestResult == "erro") {
        std::cout << "Erro:" << response.substr(resultDelimiter) << std::endl;
        return false;
    }
    return true;
}

void handleUserConnectCommand(string command) {
    if (isUserLoggedIn) {
        std::cerr << "Você já está logado." << std::endl;
        return;
    }
    
    std::regex commandValidator("(adduser|login) [A-z0-9]{3,16} [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando invalido. Tente novamente" << std::endl;
        return;
    }
    
    write(clientServerFD, command.c_str(), command.length());
    if (wasRequestSuccessful()) {
        std::cout << "Login realizado com sucesso!\nAgora você pode convidar/ser convidado para uma partida." << std::endl;
        isUserLoggedIn = true;
    }
}

void handlePasswdCommand(string command) {
    if (!isUserLoggedIn) {
        std::cerr << "Você deve estar logado para trocar sua senha." << std::endl;
        return;
    }
    
    std::regex commandValidator("(passwd) [A-z0-9]{3,16} [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando invalido. Tente novamente" << std::endl;
        return;
    }
    
    write(clientServerFD, command.c_str(), command.length());
    if (wasRequestSuccessful()) {
        std::cout << "Senha trocada com sucesso." << std::endl;
    }
}

void handleListCommand(string command) {
    string response;
    
    if (!isUserLoggedIn) {
        std::cerr << "Você deve estar logado para ver os usuários ativos." << std::endl;
        return;
    }

    write(clientServerFD, command.c_str(), command.length());
    response = getServerResponse();
    if (response.substr(0, 4) == "erro") {
        std::cerr << response.substr(4) << std::endl;
        return;
    }
    std::cout << response.substr(7) << std::endl;
}

void handleLogoutCommand(string command) {
    if (!isUserLoggedIn) {
        std::cerr << "Você não está logado." << std::endl;
        return;
    }
    
    write(clientServerFD, command.c_str(), command.length());
    if (wasRequestSuccessful()) {
        std::cout << "Logout realizado com sucesso." << std::endl;
        isUserLoggedIn = false;
    }
}

void handleExitCommand(string command) {
    write(clientServerFD, command.c_str(), command.length());
    if (wasRequestSuccessful()) {
        std::cout << "Saindo do jogo..." << std::endl;
    }

    close(clientServerFD);
    isClientConnected = false;
}

void sendInitialInfoToServer() {
    struct sockaddr_in p2pSocketAddr;
    unsigned int sockLen;
    unsigned short p2pPort;

    bzero(&p2pSocketAddr, sizeof(p2pSocketAddr));
    sockLen = sizeof(p2pSocketAddr);
    getsockname(p2pFD, (struct sockaddr *) &p2pSocketAddr, &sockLen);
    p2pPort = ntohs(p2pSocketAddr.sin_port);

    std::string info = "info " + std::to_string(p2pPort);
    write(clientServerFD, info.c_str(), info.length());
    while (!wasRequestSuccessful()) {
        write(clientServerFD, info.c_str(), info.length());
    }
}

void checkPendingInvite() {
    int inviteFD;
    char buffer[MAXLINE + 1];
    ssize_t n;

    if ((inviteFD = accept(p2pFD, (struct sockaddr *) NULL, NULL)) >= 0) {
        if ( (n=read(inviteFD, buffer, MAXLINE)) > 0) {
            buffer[n] = '\0';
            std::cout << "New invite: " << buffer << std::endl;
        }
        close(inviteFD);
    }
}

void handleClientCommand() {
    string fullCommand, command;
    int commandDelimiter;

    getline(std::cin, fullCommand);
    commandDelimiter = fullCommand.find(' ');
    command = commandDelimiter > 0 
        ? fullCommand.substr(0, commandDelimiter) 
        : fullCommand;

    if (COMMAND_TYPES.count(command) > 0) {
        switch (COMMAND_TYPES[command]) {
            case 0:
            case 2:
                handleUserConnectCommand(fullCommand);
                break;
            case 1:
                handlePasswdCommand(fullCommand);
                break;
            case 3:
                std::cout << "leaders" << std::endl;
                break;
            case 4:
                handleListCommand(command);
                break;
            case 5:
                std::cout << "begin" << std::endl;
                break;
            case 6:
                std::cout << "send" << std::endl;
                break;
            case 9:
                handleLogoutCommand(command);
                break;
            case 10:
                handleExitCommand(command);
                break;
        }
    }
}

void handleConnectedClient() {
    sendInitialInfoToServer();

    while (isClientConnected) {
        checkPendingInvite();
        std::cout << PROMPT;
        handleClientCommand();
    }
}

int main(int argc, char **argv) {
    establishServerConnection(argc, argv);

    initP2PListener();

    handleConnectedClient();

    exit(0); 
}