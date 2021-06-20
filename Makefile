CPPFLAGS=-std=c++17 -Wall -Werror
GAMENAME=jogo-da-velha
UTILS=./src/utils.cpp
SERVERSRC=$(UTILS) ./src/servidor/log_actions.cpp ./src/servidor/actions.cpp ./src/servidor/mac0352-servidor.cpp
CLIENTSRC=$(UTILS) ./src/cliente/board.cpp ./src/cliente/mac0352-cliente-jogo-da-velha.cpp

all: server client

server:
	g++ $(CPPFLAGS) $(SERVERSRC) -o ./bin/servidor/servidor-$(GAMENAME)

client:
	g++ $(CPPFLAGS) $(CLIENTSRC) -o ./bin/cliente/$(GAMENAME)