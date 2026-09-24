all:
	@g++ -std=c++20 ./main.cpp -o ./src/main

teste:
	@./src/main -i inputfile.txt
	@xxd -g 1 a.out

clean:
	@rm -rf a.out probably_inputfile.txt ./src/main

debug:
	@g++ -std=c++20 -g3 -Wall -fno-inline ./main.cpp -o ./src/main