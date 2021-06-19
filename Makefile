CPPFLAGS=-std=c++17 -Wall -Werror
GAMENAME=jogo-da-velha
LIBFLAGS= -I /usr/local/ssl/include -L /usr/local/ssl/lib -lssl -lcrypto

server:
	g++ $(CPPFLAGS) utils.cpp ./servidor/log_actions.cpp ./servidor/actions.cpp ./servidor/mac0352-servidor.cpp -o servidor/servidor-$(GAMENAME) $(LIBFLAGS)
client:
	g++ $(CPPFLAGS) utils.cpp ./cliente/mac0352-cliente-jogo-da-velha.cpp -o $(GAMENAME) $(LIBFLAGS)