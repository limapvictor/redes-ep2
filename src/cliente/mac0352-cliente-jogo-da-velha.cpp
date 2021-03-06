#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <regex>
#include <thread>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../utils.hpp"
#include "./board.hpp"

using namespace std;

#define MAXLINE 4096
#define LISTENQ 1

int clientServerFD;
int p2pFD;
int invitesFD;
int heartbeatsFD;
int unansweredInviteFD;

int savedArgc;
char **savedArgv;

SSL_CTX* ctx;

bool isClientConnected = false;
bool isUserLoggedIn = false;
bool isPlaying = false;
bool hasUnansweredInvite = false;

string loggedUser;

string currentPlayerSymbol;
string currentOpponentSymbol;

string unansweredInviter;
string pendingMessageToServer;

vector<uint64_t> currentGameDelay(5);
short int currentGameCalculatedDelaysCount;

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
    {"exit", 10},
    {"accept", 11}
};

int establishServerConnection(int argc, char **argv);
bool establishP2PConnection(string peerIpAddress, string peerPort);
void initListener(int *listenerFD);
string getServerResponse();
bool wasRequestSuccessful();
void handleGame(bool firstPlayer);
void handleInvalidCommand();
void handleInviteResponse();
void sendInitialInfoToServer();
void checkGameEnd(bool myPlay);
void handleOpponentSendCommand(vector<string> command);
void handleOpponentEndCommand();
void waitForOpponentPlay();
void updateDelayHistory(uint64_t sendTime);
void handleSendCommand(vector<string> command, string fullCommand);
void handleEndCommand(string command);
void handleDelayCommand();
void waitForInviterConnection(string inviter);
void listenForHeartbeats();
void listenForInvites();
void handleUserConnectCommand(string command);
void handlePasswdCommand(string command);
void handleLeadersCommand(string command);
void handleListCommand(string command);
void handleBeginCommand(string command);
void handleLogoutCommand(string command);
void handleExitCommand(string command);
void handleClientCommand();
void handleConnectedClient();


void createClientSSLContext() {
    const SSL_METHOD *method;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_cipher_list(ctx, "ECDHE-RSA-AES128-GCM-SHA256");
    return;
}

string getServerResponse() {
    char buffer[MAXLINE + 1];
    int bufferSize;

    if ( (bufferSize = read(clientServerFD, buffer, MAXLINE)) <= 0) {
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

void updateDelayHistory(uint64_t sendTime) {
    char buffer[MAXLINE + 1];
    ssize_t n;

    if ( (n = read(p2pFD, buffer, MAXLINE)) < 0) return;
    buffer[n] = '\0';
    vector<string> response = convertAndSplit(buffer);
    
    uint64_t receiveTime = stoull(response[1]);
    currentGameDelay[currentGameCalculatedDelaysCount++] = receiveTime - sendTime;
}

void checkGameEnd(bool myPlay) {
    string resultToServer;
    int gameResult;

    if ( (gameResult = getGameCurrentResult()) < 0) return;
    
    if (gameResult == 0) {
        std::cout << "Jogo EMPATADO!" << std::endl;
        resultToServer = myPlay ? "result draw" : "endgame";
    } else if (gameResult == getIntFromPlayerSymbol(currentPlayerSymbol)) {
        std::cout << "VIT??RIA!!" << std::endl;
        resultToServer = "result victory";
    } else {
        std::cout << "Vit??ria do OPONENTE!!" << std::endl;
        resultToServer = "endgame";
    }
    
    write(clientServerFD, resultToServer.c_str(), resultToServer.length());
    while (!wasRequestSuccessful()) {
        pendingMessageToServer = resultToServer;
        write(clientServerFD, resultToServer.c_str(), resultToServer.length());
    }

    isPlaying = false;
}

void sendInitialInfoToServer() {
    struct sockaddr_in invitesSocketAddr;
    unsigned int sockLen;
    unsigned short invitesPort, heartbeatsPort;

    bzero(&invitesSocketAddr, sizeof(invitesSocketAddr));
    sockLen = sizeof(invitesSocketAddr);
    getsockname(invitesFD, (struct sockaddr *) &invitesSocketAddr, &sockLen);
    invitesPort = ntohs(invitesSocketAddr.sin_port);
    getsockname(heartbeatsFD, (struct sockaddr *) &invitesSocketAddr, &sockLen);
    heartbeatsPort = ntohs(invitesSocketAddr.sin_port);

    std::string info = "info " + std::to_string(heartbeatsPort) + " " + std::to_string(invitesPort);
    write(clientServerFD, info.c_str(), info.length());
    while (!wasRequestSuccessful()) {
        write(clientServerFD, info.c_str(), info.length());
    }
}

int establishServerConnection(int argc, char **argv) {
    struct sockaddr_in serverAddress;

    SSL_library_init();
    createClientSSLContext();
   
    if (argc != 3) {
        fprintf(stderr, "O cliente do jogo da velha deve ser chamado com: %s <SERVER IP ADDRESS> <SERVER PORT>\n", argv[0]);
        return 1;
    }
   
    if ( (clientServerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr,"Erro interno de rede. Tente novamente mais tarde.\n");
        return 2;
    }
   
   bzero(&serverAddress, sizeof(serverAddress));
   serverAddress.sin_family = AF_INET;
   serverAddress.sin_port = htons(std::stoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) <= 0) {
        fprintf(stderr,"Erro interno de rede. Tente novamente mais tarde.\n");
        return 7;
    }
   
    if (connect(clientServerFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr,"N??o foi poss??vel se conectar com o servidor. Tente novamente mais tarde,\n");
        return 3;
    }

    isClientConnected = true;
    savedArgc = argc; savedArgv = argv;
    return 0;
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
        std::cerr << "N??o foi poss??vel estabelecer conex??o com o convidado. Tente novamente mais tarde" << std::endl;
        return false;
    }
    return true;
}

void initListener(int *listenerFD) {
    struct sockaddr_in clientAddress;

    if ( (*listenerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(4);
    }
    fcntl(*listenerFD, F_SETFL, O_NONBLOCK);

    bzero(&clientAddress, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddress.sin_port = htons(0);

    if (bind(*listenerFD, (struct sockaddr *) &clientAddress, sizeof(clientAddress)) < 0 ) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(5);
    }

    if (listen(*listenerFD, LISTENQ) < 0) {
        std::cerr << "Erro interno de rede. Tente novamente mais tarde." << std::endl;
        std::exit(6);
    }
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
            hasUnansweredInvite = false;
            currentPlayerSymbol = gameInfo[1];
            currentOpponentSymbol = inverseSymbol(gameInfo[1]);
            string firstPlayer = gameInfo[2];
            std::cout << "Jogo INICIADO!" << std::endl;
            resetBoard();
            string startGane = "startgame " + inviter;
            write(clientServerFD, startGane.c_str(), startGane.length());
            handleGame(firstPlayer == currentPlayerSymbol);
        }
    }
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
    
    currentPlayerSymbol = response[3];
    currentOpponentSymbol = inverseSymbol(currentPlayerSymbol);
    resetBoard();
    string firstToPlay = response[4];
    string gameInitMessage = "play " + currentOpponentSymbol + " " + firstToPlay;
    isPlaying = true;
    write(p2pFD, gameInitMessage.c_str(), gameInitMessage.length());
    handleGame(currentPlayerSymbol == firstToPlay);
}

void listenForHeartbeats() {
    while (isClientConnected) {
        char buffer[MAXLINE + 1];
        ssize_t n;
        int heartbeatFD;

        if ( (heartbeatFD = accept(heartbeatsFD, (struct sockaddr *) NULL, NULL)) >= 0) {
            if ( (n = read(heartbeatFD, buffer, MAXLINE)) > 0) {
                buffer[n] = '\0';

                write(heartbeatFD, buffer, n);
            }
            close(heartbeatFD);
        }
    }
    close(heartbeatsFD);
}

void listenForInvites() {
    int inviteFD;
    char buffer[MAXLINE + 1];
    ssize_t n;

    while (isClientConnected) {
        while (hasUnansweredInvite || isPlaying);

        if ( (inviteFD = accept(invitesFD, (struct sockaddr *) NULL, NULL)) >= 0) {
            if ( (n = read(inviteFD, buffer, MAXLINE)) > 0) {
                buffer[n] = '\0';

                vector<string> response = convertAndSplit(buffer);
                string inviteResponse;
                if (response[0] == "invite") {
                    string inviter = response[1];
                    std::cout << "\nDesculpe pela interrup????o, mas voc?? acabou de ser convidado para um jogo por: " << inviter << std::endl;
                    std::cout << "Digite \"accept\" para aceitar o convite, qualquer outra coisa para recusar." << std::endl;
                    std::cout << PROMPT << std::flush;
                    hasUnansweredInvite = true;
                    unansweredInviteFD = inviteFD;
                    unansweredInviter = inviter;
                } else {
                    close(inviteFD);
                }
            }
        }
    }
    close(invitesFD);
}

void waitForOpponentPlay() {
    char buffer[MAXLINE + 1];
    ssize_t n;
    vector<string> response;

    std::cout << "Esperando jogada do advers??rio..." << std::endl;
    if ( (n = read(p2pFD, buffer, MAXLINE)) > 0) {
        std::cout << "Jogada recebida!" << std::endl;
        buffer[n] = '\0';
        response = convertAndSplit(buffer);
        if (response[0] == "send") handleOpponentSendCommand(response);
        else if (response[0] == "end") handleOpponentEndCommand();
    }
}

void handleGame(bool firstToPlay) {
    string gameWelcomeMessage = firstToPlay 
        ? "Voc?? ?? o primeiro a jogar. Fa??a sua jogada"
        : "Voc?? ?? o segundo a jogar. Espere a jogada do outro jogador.";
    
    std::cout << gameWelcomeMessage << std::endl;
    
    std::fill(currentGameDelay.begin(), currentGameDelay.end(), 0);
    currentGameCalculatedDelaysCount = 0;
    if (!firstToPlay) waitForOpponentPlay();
    while (isPlaying) {
        vector<string> command;
        string fullCommand;
        
        std::cout << PROMPT;
        getline(std::cin, fullCommand);
        command = convertAndSplit(fullCommand.data());

        if (command[0] == "send") handleSendCommand(command, fullCommand);
        else if (command[0] == "end") handleEndCommand(command[0]);
        else if (command[0] == "delay") handleDelayCommand();
        else handleInvalidCommand();
    }
    std::cout << "Saindo do jogo..." << std::endl;
    close(p2pFD);
}

void handleOpponentSendCommand(vector<string> command) {
    uint64_t receiveTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    string receiveIn = "receivein " + std::to_string(receiveTime);
    write(p2pFD, receiveIn.c_str(), receiveIn.length());

    updateBoard(currentOpponentSymbol, stoi(command[1]), stoi(command[2]));
    printBoard();
    checkGameEnd(false);
}

void handleOpponentEndCommand() {
    string resultToServer = "result victory";
    write(clientServerFD, resultToServer.c_str(), resultToServer.length());
    wasRequestSuccessful();
    isPlaying = false;
    std::cout << "VIT??RIA!! Seu oponente desistiu do jogo." << std::endl;
}

void handleInvalidCommand() {
    std::cout << "O comando digitado n??o existe ou n??o est?? dispon??vel no momento. Tente novamente." << std::endl;
}

void handleUserConnectCommand(string command) {
    char buffer[MAXLINE + 1];
    if (isUserLoggedIn) {
        std::cerr << "Voc?? j?? est?? logado." << std::endl;
        return;
    }
    
    std::regex commandValidator("(adduser|login) [A-z0-9]{3,16} [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando mal formatado. Ele deve ter o formato 'comando <USER(AlfaNum 3-16 carecteres)> <SENHA(AlfaNum 3-16 carecteres)>'. Tente novamente" << std::endl;
        return;
    }

    write(clientServerFD, "encrypted", 9);
    
    SSL* ssl = SSL_new(ctx);

    SSL_set_fd(ssl, clientServerFD); 
    SSL_connect(ssl);
    SSL_write(ssl, command.c_str(), command.length());
    int n = SSL_read(ssl, buffer, MAXLINE);
    buffer[n] = 0;
    SSL_shutdown(ssl);
    SSL_free(ssl);

    if (wasRequestSuccessful()) {
        std::cout << "Login realizado com sucesso!\nAgora voc?? pode convidar/ser convidado para uma partida." << std::endl;
        isUserLoggedIn = true;
        loggedUser = convertAndSplit(command.data())[1];
    }
}

void handlePasswdCommand(string command) {
    char buffer[MAXLINE + 1];
    if (!isUserLoggedIn) {
        std::cerr << "Voc?? deve estar logado para trocar sua senha." << std::endl;
        return;
    }
    
    std::regex commandValidator("(passwd) [A-z0-9]{3,16} [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando mal formatado. Ele deve ter o formato 'passwd <VELHA(AlfaNum 3-16 carecteres)> <NOVA(AlfaNum 3-16 carecteres)>'. Tente novamente" << std::endl;
        return;
    }
    
    write(clientServerFD, "encrypted", 9);
    
    SSL* ssl = SSL_new(ctx);

    SSL_set_fd(ssl, clientServerFD); 
    SSL_connect(ssl);
    SSL_write(ssl, command.c_str(), command.length());
    int n = SSL_read(ssl, buffer, MAXLINE);
    buffer[n] = 0;
    SSL_shutdown(ssl);
    SSL_free(ssl);
    if (wasRequestSuccessful()) {
        std::cout << "Senha trocada com sucesso." << std::endl;
    }
}

void handleLeadersCommand(string command) {
    string response;

    write(clientServerFD, command.c_str(), command.length());
    response = getServerResponse();
    const short int LEADERS_RESP_LENGTH = 7;
    std::cout << response.substr(LEADERS_RESP_LENGTH + 1) << std::endl;
}

void handleListCommand(string command) {
    string response;
    
    if (!isUserLoggedIn) {
        std::cerr << "Voc?? deve estar logado para ver os usu??rios ativos." << std::endl;
        return;
    }

    write(clientServerFD, command.c_str(), command.length());
    response = getServerResponse();
    if (response.substr(0, 5) == "error") {
        std::cerr << response.substr(5 + 1) << std::endl;
        return;
    }
    const short int ACTIVE_RESP_LENGTH = 6;
    std::cout << response.substr(ACTIVE_RESP_LENGTH + 1) << std::endl;
}

void handleBeginCommand(string command) {
    if (!isUserLoggedIn) {
        std::cerr << "Voc?? deve estar logado para iniciar uma partida." << std::endl;
        return;
    }

    std::regex commandValidator("(begin) [A-z0-9]{3,16}");
    if (!std::regex_match(command, commandValidator)) {
        std::cerr << "Comando mal formatado. O comando deve ser usado com 'begin <USER(AlfaNum??rico 3 a 16 caracteres)>'. Tente novamente." << std::endl;
        return;
    }

    write(clientServerFD, command.c_str(), command.length());
    std::cout << "Convite enviado para o servidor!" << std::endl;
    std::cout << "Esperando resposta..." << std::endl;
    handleInviteResponse();
}

void handleSendCommand(vector<string> command, string fullCommand) {
    if (!isPlaying) {
        std::cerr << "Voc?? deve estar jogando para usar esse comando." << std::endl;
    }
    
    std::regex commandValidator("(send) [1-3] [1-3]");
    if (!std::regex_match(fullCommand, commandValidator)) {
        std::cerr << "Comando mal formatado. Deve possuir a forma 'send <LINHA(1-3)> <COLUNA(1-3)>'.Tente novamente" << std::endl;
        return;
    }

    int line = stoi(command[1]), column = stoi(command[2]);
    if (getPlayerInPosition(line, column) != " ") {
        std::cerr << "A casa j?? est?? ocupada.Tente novamente" << std::endl;
        return;
    }
    
    updateBoard(currentPlayerSymbol, line, column);
    printBoard();

    uint64_t sendTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    write(p2pFD, fullCommand.c_str(), fullCommand.length());
    updateDelayHistory(sendTime);

    checkGameEnd(true);
    if (isPlaying) waitForOpponentPlay();
}

void handleEndCommand(string command) {
    if (!isPlaying) {
        std::cerr << "Voc?? deve estar jogando para usar esse comando." << std::endl;
    }
    
    string endgame = "endgame";
    write(p2pFD, command.c_str(), command.length());
    write(clientServerFD, endgame.c_str(), endgame.length());
    wasRequestSuccessful();
    isPlaying = false;
    std::cout << "Vit??ria do OPONENTE!! Voc?? desistiu do jogo." << std::endl;
}

void handleDelayCommand() {
    if (!isPlaying) {
        std::cerr << "Voc?? deve estar jogando para usar esse comando." << std::endl;
    }

    std::cout << "??ltimos delays aproximados (em microsegundos)" << std::endl;
    for (int i = 1; i <= 3; i++) {
        if (currentGameCalculatedDelaysCount - i < 0) break;
        std::cout << currentGameDelay[currentGameCalculatedDelaysCount - i] << std::endl;
    }
}

void handleLogoutCommand(string command) {
    if (!isUserLoggedIn) {
        std::cerr << "Voc?? n??o est?? logado." << std::endl;
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

void handleClientCommand() {
    string fullCommand, command;
    int commandDelimiter;

    getline(std::cin, fullCommand);
    commandDelimiter = fullCommand.find(' ');
    command = commandDelimiter > 0 
        ? fullCommand.substr(0, commandDelimiter) 
        : fullCommand;

    if (hasUnansweredInvite) {
        if (command == "accept") {
            write(unansweredInviteFD, command.c_str(), command.length());
            std::cout << "Estabelecendo conex??o com o jogador " << unansweredInviter << ". Aguarde..." << std::endl;
            close(unansweredInviteFD);
            waitForInviterConnection(unansweredInviter);
        } else {
            string inviteResponse = "reject";
            write(unansweredInviteFD, inviteResponse.c_str(), inviteResponse.length());
            hasUnansweredInvite = false;
        }
    }
    
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
            handleLeadersCommand(command);
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
    std::thread heartbeatsListenerThread(listenForHeartbeats);
    std::thread invitesListenerThread(listenForInvites);

    while (isClientConnected) {
        std::cout << PROMPT;
        handleClientCommand();
    }
    heartbeatsListenerThread.join();
    invitesListenerThread.join();
}

void handleLostConnectionWithServer(int signum) {
    std::cout << "Voc?? perdeu conex??o com o servidor. Tentando restabelecer conex??o com o servidor (por 3 min)" << std::endl;
    uint64_t startTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() - startTime < 180) {
        std::cout << "Tentativa de restabelecer..." << std::endl;
        if (establishServerConnection(savedArgc, savedArgv) == 0) {
            string reconnect = "reconnect " + loggedUser;
            write(clientServerFD, reconnect.c_str(), reconnect.length());
            if (wasRequestSuccessful()) {
                write(clientServerFD, pendingMessageToServer.c_str(), pendingMessageToServer.length());
                pendingMessageToServer = "";
                std::cout << "Conex??o restabelecida!" << std::endl;
                return;
            }
        }
        sleep(10);
    }
    std::cout << "N??o foi poss??vel restabelecer a conex??o no tempo limite. Saindo..." << std::endl;
    exit(10);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, handleLostConnectionWithServer);

    int established = establishServerConnection(argc, argv);
    if (established > 0) exit(established);

    initListener(&heartbeatsFD);
    
    initListener(&invitesFD);

    std::cout << "-------------------------JV's-------------------------" << std::endl;
    handleConnectedClient();

    exit(0); 
}