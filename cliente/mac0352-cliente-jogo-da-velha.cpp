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
#include <math.h>

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
int invitesFD;

bool isClientConnected = false;
bool isUserLoggedIn = false;
bool isPlaying = false;

std::map<string, int> SYMBOL_TO_INT = {
    {" ", 0},
    {"X", 1},
    {"O", 2},
};
string currentGameChar;
string currentOpponentChar;
int board = 0;

string PROMPT = "JogoDaVelha> ";

std::map<string, int> COMMAND_TYPES = {
    {"adduser", 0},
    {"passwd", 1},
    {"login", 2},
    {"leaders", 3},
    {"list", 4},
    {"begin", 5},
    {"send", 6},
    {"delay", 7},
    {"end", 8},
    {"logout", 9},
    {"exit", 10}
};

void handleGame(bool firstPlayer);

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

bool establishP2PConnection(string peerIpAddress, string peerPort) {
    struct sockaddr_in peerAddress;
   
    if ( (p2pFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde" << std::endl;
        return false;
    }
   
   bzero(&peerAddress, sizeof(peerAddress));
   peerAddress.sin_family = AF_INET;
   peerAddress.sin_port = htons(std::stoi(peerPort.c_str()));

    if (inet_pton(AF_INET, peerIpAddress.c_str(), &peerAddress.sin_addr) <= 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde" << std::endl;
        return false;
    }
   
    if (connect(p2pFD, (struct sockaddr *) &peerAddress, sizeof(peerAddress)) < 0) {
        std::cerr << "Não foi possível estabelecer conexão com o convidado. Tente novamente mais tarde" << std::endl;
        return false;
    }
    return true;
}

void initP2PListener() {
    struct sockaddr_in clientAddress;

    if ( (invitesFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(4);
    }
    fcntl(invitesFD, F_SETFL, O_NONBLOCK);

    bzero(&clientAddress, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddress.sin_port = htons(0);

    if (bind(invitesFD, (struct sockaddr *) &clientAddress, sizeof(clientAddress)) < 0 ) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(5);
    }

    if (listen(invitesFD, LISTENQ) < 0) {
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

    if (requestResult == "error") {
        std::cout << "Erro:" << response.substr(resultDelimiter) << std::endl;
        return false;
    }
    return true;
}

string inverseSymbol(string symbol) {
    return symbol == "X" ? "O" : "X";
}

string getSymbolFromInt(int symbolAsInt) {
    for (const auto& [key, value] : SYMBOL_TO_INT) {
        if (value == symbolAsInt) return key;
    }
    return " ";
}

string getPlayerInPosition(int line, int column) {
    int player;

    player = fmod(board / pow(3, 3 * line + column), 3);
    return getSymbolFromInt(player);
}

int isSequenceComplete(int x, int y, int z) {
    if (x == y && y == z) return x;
    return 0;
}

int getGameCurrentResult() {
    int posA, posB, posC, posD, posE, posF, posG, posH, posI;
    int lines, columns, diagonal1, diagonal2;

    posA = fmod(board / pow(3, 0), 3);
    posB = fmod(board / pow(3, 1), 3);
    posC = fmod(board / pow(3, 2), 3);
    posD = fmod(board / pow(3, 3), 3);
    posE = fmod(board / pow(3, 4), 3);
    posF = fmod(board / pow(3, 5), 3);
    posG = fmod(board / pow(3, 6), 3);
    posH = fmod(board / pow(3, 7), 3);
    posI = fmod(board / pow(3, 8), 3);

    lines = isSequenceComplete(posA, posB, posC) + isSequenceComplete(posD, posE, posF) + isSequenceComplete(posG, posH, posI);
    if (lines > 0) return lines;
    columns = isSequenceComplete(posA, posD, posG) + isSequenceComplete(posB, posE, posH) + isSequenceComplete(posC, posF, posI);
    if (columns > 0) return columns;
    diagonal1 = isSequenceComplete(posA, posE, posI);
    if (diagonal1 > 0) return diagonal1;
    diagonal2 = isSequenceComplete(posC, posE, posG);
    if (diagonal2 > 0) return diagonal2;

    if (posA > 0 && posB > 0 && posC > 0 && posD > 0 && posE > 0 && posF > 0 && posG > 0 && posH > 0 && posI > 0) return 0;
    return -1;
}

void updateBoard(string player, int line, int column) {
    board += pow(3, (3 * (line - 1) + (column - 1))) * (SYMBOL_TO_INT[player]);
    std::cout << "Tabuleiro atualizado: " << std::endl;
}

void printBoard() {
    std::cout << " " << getPlayerInPosition(0, 0) << " " << "|" << " " << getPlayerInPosition(0, 1) << " " << "|" << " " << getPlayerInPosition(0, 2) <<std::endl;
    std::cout << "---+---+---" << std::endl;
    std::cout << " " << getPlayerInPosition(1, 0) << " " << "|" << " " << getPlayerInPosition(1, 1) << " " << "|" << " " << getPlayerInPosition(1, 2) <<std::endl;
    std::cout << "---+---+---" << std::endl;
    std::cout << " " << getPlayerInPosition(2, 0) << " " << "|" << " " << getPlayerInPosition(2, 1) << " " << "|" << " " << getPlayerInPosition(2, 2) <<std::endl;
}

void handleInvalidCommand() {
    std::cout << "O comando digitado não existe ou não está disponível no momento. Tente novamente." << std::endl;
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

void handleInviteResponse() {
    char buffer[MAXLINE + 1];
    int bufferSize;

    if ( (bufferSize = read(clientServerFD, buffer, MAXLINE)) < 0) {
        std::cerr << "Erro ao ler a resposta do servidor. Tente novamente." << std::endl;
        return;
    }

    buffer[bufferSize] = '\0';
    vector<string> response = convertAndSplit(buffer);
    if (response[0] == "error" || response[0] == "reject") {
        string error(buffer);
        error = error.substr(response[0].length());
        std::cerr << error << std::endl;
        return;
    }
    if (!establishP2PConnection(response[1], response[2])) return;
    
    currentGameChar = response[3];
    currentOpponentChar = inverseSymbol(currentGameChar);
    string firstToPlay = response[4];
    string gameInitMessage = "play " + currentOpponentChar + " " + firstToPlay;
    isPlaying = true;
    write(p2pFD, gameInitMessage.c_str(), gameInitMessage.length());
    handleGame(currentGameChar == firstToPlay);
}

void handleBeginCommand(string command) {
    if (!isUserLoggedIn) {
        std::cerr << "Você deve estar logado para iniciar uma partida." << std::endl;
        return;
    }

    std::regex commandValidator("(begin) [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando invalido. Tente novamente" << std::endl;
        return;
    }

    write(clientServerFD, command.c_str(), command.length());
    std::cout << "Convite enviado para o servidor!" << std::endl;
    std::cout << "Esperando resposta..." << std::endl;
    handleInviteResponse();
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
    struct sockaddr_in invitesSocketAddr;
    unsigned int sockLen;
    unsigned short invitesPort;

    bzero(&invitesSocketAddr, sizeof(invitesSocketAddr));
    sockLen = sizeof(invitesSocketAddr);
    getsockname(invitesFD, (struct sockaddr *) &invitesSocketAddr, &sockLen);
    invitesPort = ntohs(invitesSocketAddr.sin_port);

    std::string info = "info " + std::to_string(invitesPort);
    std::cout << info << std::endl;
    write(clientServerFD, info.c_str(), info.length());
    while (!wasRequestSuccessful()) {
        write(clientServerFD, info.c_str(), info.length());
    }
}

void checkGameEnd(bool myPlay) {
    int gameResult;
    string resultToServer;

    if ( (gameResult = getGameCurrentResult()) < 0) return;
    
    if (gameResult == 0) {
        std::cout << "Jogo EMPATADO!" << std::endl;
        resultToServer = myPlay ? "result draw" : "endgame";
    } else if (gameResult == SYMBOL_TO_INT[currentGameChar]) {
        std::cout << "VITÓRIA!!" << std::endl;
        resultToServer = "result victory";
    } else {
        std::cout << "Vitória do OPONENTE!!" << std::endl;
        resultToServer = "endgame";
    }
    
    write(clientServerFD, resultToServer.c_str(), resultToServer.length());
    wasRequestSuccessful();

    isPlaying = false;
    std::cout << "Saindo do jogo..." << std::endl;
}

void waitForOpponentPlay() {
    char buffer[MAXLINE + 1];
    ssize_t n;
    vector<string> response;

    std::cout << "Esperando jogada do adversário..." << std::endl;
    if ( (n = read(p2pFD, buffer, MAXLINE)) > 0) {
        std::cout << "Jogada recebida!" << std::endl;
        buffer[n] = '\0';
        response = convertAndSplit(buffer);
        updateBoard(currentOpponentChar, stoi(response[1]), stoi(response[2]));
        printBoard();
        checkGameEnd(false);
    }
}

void handleSendCommand(vector<string> command, string fullCommand) {
    updateBoard(currentGameChar, stoi(command[1]), stoi(command[2]));
    printBoard();
    write(p2pFD, fullCommand.c_str(), fullCommand.length());
    checkGameEnd(true);
    if (isPlaying) waitForOpponentPlay();
}

void handleEndCommand(string command) {
    write(p2pFD, command.c_str(), command.length());
    isPlaying = false;
    std::cout << "Saindo do jogo..." << std::endl;
}

void handleGame(bool firstToPlay) {
    string gameWelcomeMessage = firstToPlay 
        ? "Você é o primeiro a jogar. Faça sua jogada"
        : "Você é o segundo a jogar. Espere a jogada do outro jogador.";
    
    std::cout << gameWelcomeMessage << std::endl;
    
    if (!firstToPlay) waitForOpponentPlay();
    while (isPlaying) {
        vector<string> command;
        string fullCommand;
        
        std::cout << PROMPT;
        getline(std::cin, fullCommand);
        command = convertAndSplit(fullCommand.data());

        if (command[0] == "send") handleSendCommand(command, fullCommand);
        else if (command[0] == "end") handleEndCommand(command[0]);
        // else if (command[0] == "delay") handleDelayCommand(command[0]);
        else handleInvalidCommand();
    }
    close(p2pFD);
}

void waitForInviterConnection(string inviter) {
    char buffer[MAXLINE + 1];
    ssize_t n;

    while ( (p2pFD = accept(invitesFD, (struct sockaddr *) NULL, NULL)) < 0);
    if ( (n = read(p2pFD, buffer, MAXLINE)) > 0) {
        buffer[n] = '\0';

        vector<string> gameInfo = convertAndSplit(buffer);
        if (gameInfo[0] == "play") {
            isPlaying = true;
            board = 0;
            currentGameChar = gameInfo[1];
            currentOpponentChar = inverseSymbol(gameInfo[1]);
            string firstPlayer = gameInfo[2];
            std::cout << "Jogo INICIADO!" << std::endl;
            string startGane = "startgame " + inviter;
            write(clientServerFD, startGane.c_str(), startGane.length());
            handleGame(firstPlayer == currentGameChar);
        }
    }
}

void checkPendingInvite() {
    int inviteFD;
    char buffer[MAXLINE + 1];
    ssize_t n;

    if ( (inviteFD = accept(invitesFD, (struct sockaddr *) NULL, NULL)) >= 0) {
        if ( (n = read(inviteFD, buffer, MAXLINE)) > 0) {
            buffer[n] = '\0';

            vector<string> response = convertAndSplit(buffer);
            string inviteResponse;
            if (response[0] == "invite") {
                string inviter = response[1];
                std::cout << "Você foi convidado para um jogo por: " << inviter << std::endl;
                std::cout << "Digite \"aceitar\" para aceitar o convite, qualquer outra coisa para recusar." << std::endl;
                std::cout << PROMPT;
                getline(std::cin, inviteResponse);
                if (inviteResponse == "aceitar") {
                    inviteResponse = "accept";
                    std::cout << inviteResponse.c_str() << std::endl;
                    write(inviteFD, inviteResponse.c_str(), inviteResponse.length());
                    std::cout << "Estabelecendo conexão com o jogador " << inviter << ". Aguarde..." << std::endl;
                    close(inviteFD);
                    waitForInviterConnection(inviter);
                } else {
                    inviteResponse = "reject";
                    write(inviteFD, inviteResponse.c_str(), inviteResponse.length());
                }
            };
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


    if (COMMAND_TYPES.count(command) <= 0) {
        handleInvalidCommand();
        return;
    }
    
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
            handleBeginCommand(fullCommand);
            break;
        case 9:
            handleLogoutCommand(command);
            break;
        case 10:
            handleExitCommand(command);
            break;
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