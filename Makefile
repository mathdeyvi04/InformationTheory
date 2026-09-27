CXX          := g++
CXXFLAGS     := -std=c++20
DEBUGFLAGS   := -g3 -Wall -fno-inline -fno-inline-small-functions
PYTHON       := python3
PYTHONFLAGS  := -m

SRC          := ./src/main.cpp
BIN_DIR 	 := ./bin
BIN          := $(BIN_DIR)/Compressor
TEST         := tests.main

# Número correspondente do Algoritmo para straight_test ou do deep_test que será realizado
N            ?= 0

BUILD_DIR    := ./build
# Entrada padrão do teste
INPUT        ?= $(BUILD_DIR)/inputfile.txt
# Saídas geradas pelo compressor
COMPRESSED   := $(BUILD_DIR)/a.out
DECOMPRESSED := $(BUILD_DIR)/probably_inputfile.txt

.PHONY: all debug deep_test straight_test clean

all:
	@mkdir -p $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

debug:
	@mkdir -p $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(SRC) -o $(BIN)

deep_test:
	@$(PYTHON) $(PYTHONFLAGS) $(TEST) -n $(N)

straight_test:
	@mkdir -p $(BUILD_DIR)
	@echo "════════════════════════════════════════"
	@echo "             Compressão"
	@echo "════════════════════════════════════════"
	@$(BIN) -i $(INPUT) -o $(COMPRESSED) -m 2 -n $(N)
	@mv probably_inputfile.txt $(DECOMPRESSED)

	@echo
	@echo "════════════════════════════════════════"
	@echo "           Dump hexadecimal"
	@echo "════════════════════════════════════════"
	@xxd -g 1 $(COMPRESSED)

	@echo
	@echo "════════════════════════════════════════"
	@echo "                Tamanhos"
	@echo "════════════════════════════════════════"
	@original=$$(du -b $(INPUT)); \
	 comprimido=$$(du -b $(COMPRESSED)); \
	 echo " Original  : $$original bytes"; \
	 echo " Comprimido: $$comprimido bytes"

	@echo
	@echo "════════════════════════════════════════"
	@echo "            Verificação diff"
	@echo "════════════════════════════════════════"
	@diff $(INPUT) $(DECOMPRESSED) \
		&& echo " OK: arquivos idênticos" \
		|| echo " ERRO: arquivos diferem"

clean:
	@rm -rf $(COMPRESSED) $(DECOMPRESSED) $(BIN)