## Sumário

- [Algoritmos disponíveis](#algoritmos-disponíveis)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Compilação](#compilação)
- [Uso](#uso)
  - [Opções](#opções)
  - [Exemplos](#exemplos)
- [Testes](#testes)
  - [Testes automatizados — `deep_test`](#testes-automatizados--deep_test)
  - [Teste manual — `straight_test`](#teste-manual--straight_test)
  - [Limpeza](#limpeza)

---

# InformationTheory

Implementações em **C++** de algoritmos clássicos de **Teoria da Informação** aplicados à **compressão de dados sem perdas**. O projeto serve como material de estudo e referência: cada algoritmo é implementado de forma independente, documentado e submetido a uma bateria de testes empíricos (taxa de compressão, tempo de execução e uso de memória).

---

## Algoritmos disponíveis

| # | Algoritmo | Tipo | Status |
|:-:|-----------|------|:------:|
| 0 | **Huffman** | Codificação por árvore | ✅ |
| 1 | **Shannon-Fano-Elias** | Codificação aritmética por intervalos | ✅ |
| 2 | **Lempel-Ziv-Welch (LZW)** | Dicionário dinâmico | ✅ |

O número da coluna **#** corresponde ao valor passado por `-n` na linha de comando.

---

## Estrutura do repositório

```
InformationTheory/
├── bin/                    # executável compilado
├── build/                  # arquivos gerados em tempo de execução
├── src/
│   ├── LossLessCompression/    # implementação de cada algoritmo
│   ├── core/                   # interface base e abstração de arquivo
│   ├── util/                   # dependências úteis
│   └── main.cpp
├── tests/                  # suíte de testes em Python
│   ├── Compressibility/        # taxa de compressão por algoritmo
│   ├── MaxCompressibility/     # compressão iterativa (até 5 execuções)
│   ├── TimeDuration/           # tempo médio de compressão e descompressão
│   ├── MemoryUsage/            # perfil de memória via Valgrind Massif
│   ├── core/                   # classe-base de teste
│   └── test_files/             # corpus de referência
├── third_party/            # dependências externas (cxxopts)
└── Makefile
```

---

## Compilação

Requer **g++** com suporte a **C++20** e o cabeçalho `cxxopts.hpp` em `third_party/`.

```bash
make          # build otimizado
make debug    # build com símbolos de depuração (para Valgrind ou gdb)
```

O binário é gerado em `bin/Compressor`.

---

## Uso

```bash
./bin/Compressor -i <entrada> -o <saída> -n <algoritmo> -m <modo>
```

### Opções

| Opção | Longa | Descrição | Padrão |
|:-----:|-------|-----------|:------:|
| `-i` | `--inputfile` | Arquivo de entrada | — (obrigatório) |
| `-o` | `--outputfile` | Arquivo de saída | `a.out` |
| `-n` | `--number` | Algoritmo (0=Huffman, 1=Shannon, 2=LZW) | `0` |
| `-m` | `--mode` | 0=comprimir, 1=descomprimir, 2=ambos | `0` |
| `-h` | `--help` | Mostra a ajuda | — |

### Exemplos

```bash
# Comprimir texto.txt com Huffman
./bin/Compressor -i texto.txt -o texto.huff -n 0 -m 0

# Descomprimir com LZW
./bin/Compressor -i texto.lzw -o texto.txt -n 2 -m 1

# Comprimir e descomprimir em sequência, medindo tempo (modo 2)
./bin/Compressor -i texto.txt -o texto.bin -n 1 -m 2
```

---

## Testes

O projeto oferece **duas formas** de testar o binário, ambas controladas pelo Makefile.

### Testes automatizados — `deep_test`

Executa a suíte em Python (`tests.main`), que percorre o corpus de referência e mede o comportamento dos algoritmos sob **quatro perspectivas**:

| # | Teste | O que mede |
|:-:|-------|-----------|
| 0 | **Compressibility** | Taxa de compressão (tamanho final ÷ original) por arquivo e algoritmo |
| 1 | **MaxCompressibility** | Efeito da recompressão iterativa (até 5 execuções sucessivas) |
| 2 | **TimeDuration** | Tempo médio de compressão e descompressão (média de 5 execuções) |
| 3 | **MemoryUsage** | Perfil de memória (heap e stack) via Valgrind Massif |

Uso:

```bash
make deep_test N=<número_do_teste>
```

Exemplos:

```bash
make deep_test N=0    # taxa de compressão
make deep_test N=2    # tempo de execução
make deep_test N=3    # uso de memória
```

Cada teste gera `results.csv` e `results.png` no diretório correspondente em `tests/`.

> **Pré-requisitos para o teste de memória (N=3):** ter o **Valgrind** instalado e o binário compilado com `make debug`.

### Teste manual — `straight_test`

Executa uma única compressão seguida de descompressão sobre um arquivo de entrada, e exibe no terminal:

1. Tempo de compressão e descompressão (via modo `-m 2`).
2. Dump hexadecimal do arquivo comprimido (`xxd`).
3. Tamanho original vs. tamanho comprimido.
4. Verificação de integridade (`diff` entre original e resultado da descompressão).

Uso:

```bash
make straight_test N=<algoritmo>
```

Exemplos:

```bash
make straight_test N=0    # Huffman
make straight_test N=1    # Shannon-Fano-Elias
make straight_test N=2    # LZW
```

Por padrão, o teste usa `build/inputfile.txt` como entrada. Para testar outro arquivo:

```bash
make straight_test N=2 INPUT=./caminho/para/arquivo.txt
```

Os artefatos intermediários são gravados em `build/`:
- `build/a.out` — arquivo comprimido
- `build/probably_inputfile.txt` — resultado da descompressão

### Limpeza

```bash
make clean    # remove binário e artefatos de execução
```