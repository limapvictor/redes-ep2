CPPFLAGS=-std=c++17 -Wall -Werror
GAMENAME=jogo-da-velha

server:
	g++ $(CPPFLAGS) utils.cpp ./servidor/actions.cpp ./servidor/mac0352-servidor.cpp -o servidor-$(GAMENAME)
client:
	g++ $(CPPFLAGS) ./cliente/mac0352-cliente-jogo-da-velha.cpp -o $(GAMENAME)