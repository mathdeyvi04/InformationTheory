# Information Theory

Implementações em C++ de algoritmos clássicos de **Teoria da Informação**, com foco em **compressão de dados**.

O objetivo do repositório é servir como material de estudo e referência: cada algoritmo é implementado de forma independente, documentado e acompanhado de exemplos de uso.

---

## Algoritmos implementados

| # | Algoritmo | Status | Descrição |
|:-:|-----------|:------:|-----------|
| 0 | **Codificação de Huffman** | ✅ Implementado | Constrói uma árvore binária a partir das frequências dos símbolos; os mais frequentes recebem códigos mais curtos. |
| 1 | **Shannon-Fano-Elias** | 🚧 Em desenvolvimento | Codificação aritmética por intervalos cumulativos de probabilidade. |
| 2 | **Lempel-Ziv (LZ77/LZ78)** | 🚧 Em desenvolvimento | Compressão baseada em dicionário e referências a ocorrências anteriores. |
| 3 | **Lempel-Ziv-Welch (LZW)** | 🚧 Em desenvolvimento | Variante do LZ com dicionário construído dinamicamente durante a codificação. |

> Os números da coluna **#** correspondem ao valor passado pela opção `-n` na linha de comando.

---

## Estrutura do projeto

```
InformationTheory/
├── src/
│   ├── CompressionAlgorithm.hpp      # Interface abstrata base
│   ├── LossLessCompression/
│   │   ├── Huffman.hpp
│   │   ├── ShannonFanoElias.hpp
│   │   ├── LempelZiv.hpp
│   │   └── LempelZivWelch.hpp
│   └── all_includes.hpp
├── main.cpp
└── Makefile
```

Todos os algoritmos herdam de `CompressionAlgorithm`, que define a interface:

```c++
virtual std::vector<uint8_t> apply(const std::vector<uint8_t>& data) = 0;
virtual std::vector<uint8_t> deapply(const std::vector<uint8_t>& data) = 0;
```

---

## Comandos de Terminal

Requer um compilador com suporte a **C++17** ou superior.

```bash
make
```

Isso gera o binário `Compressor`.

```bash
make debug
```

Isso gera um binário `Compressor` apropriado para utilização em `gdb`.

```bash
make tese
```

Executa uma bateria de testes sobre os arquivos.

---

## Uso

```bash
./Compressor -i <entrada> -o <saída> -n <algoritmo> -m <modo>
```

### Opções

| Opção curta | Opção longa |                   Descrição                        |
|:-----------:|-------------|----------------------------------------------------|
| `-i` | `--inputfile` |               Nome ou caminho do arquivo de entrada |
| `-o` | `--outputfile` |                 Nome ou caminho do arquivo de saída |
| `-n` | `--number` |    Número correspondente ao algoritmo de compressão |
| `-m` | `--mode` | Modo de Operação (Compressão, Descompressão, Ambos) |
| `-h` | `--help` |                                      Mostra a ajuda |

### Algoritmos disponíveis

| Valor de `-n` | Algoritmo |
|:-------------:|-----------|
| `0` | Codificação de Huffman |
| `1` | Shannon-Fano-Elias |
| `2` | Lempel-Ziv |
| `3` | Lempel-Ziv-Welch |

---

## Exemplos

### Comprimir um arquivo com Huffman

```bash
./Compressor -i texto.txt -o texto.huff -n 0
```

### Descomprimir com LZW

```bash
./Compressor -i imagem.bmp -o imagem.lzw -n 3 -m 1
```

### Ver a ajuda

```bash
./Compressor -h
```

---

## Formato dos arquivos comprimidos

Cada algoritmo gera um arquivo próprio, com seu cabeçalho contendo os metadados necessários para a descompressão (frequências, tabela de códigos, dicionário, etc.).

> ⚠️ **Nota:** os formatos ainda estão em evolução. Arquivos gerados por versões antigas podem não ser compatíveis com versões futuras.
---
