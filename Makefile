all:
	@g++ -std=c++20 ./main.cpp -o ./src/Compressor

teste:
	@./src/Compressor -i ./inputfile.txt -m 2 -n 2
	@echo
	@echo "═══════════════════════════════════════"
	@echo "           Dump hexadecimal"
	@echo "═══════════════════════════════════════"
	@xxd -g 1 ./a.out
	@echo
	@echo "════════════════════════════════════════"
	@echo "                Tamanhos"
	@echo "════════════════════════════════════════"
	@original=$$(du -b ./inputfile.txt); \
	 comprimido=$$(du -b ./a.out); \
	 echo " Original  : $$original bytes"; \
	 echo " Comprimido: $$comprimido bytes";
	@echo
	@echo "════════════════════════════════════════"
	@echo "            Verificação diff"
	@echo "════════════════════════════════════════"
	@diff ./inputfile.txt ./probably_inputfile.txt && echo "\tOK: arquivos idênticos" \
	 || echo "\tERRO: arquivos diferem"

clean:
	@rm -rf ./a.out ./probably_inputfile.txt ./src/Compressor

debug:
	@g++ -std=c++20 -g3 -Wall -fno-inline ./main.cpp -o ./src/Compressor