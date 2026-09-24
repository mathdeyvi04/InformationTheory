#include "./src/all_includes.hpp"
#include <string>
#include <iostream>
#include <memory>
#include <functional>

// Criamos um vetor com todas as possibilidades de algoritmos
using Factory = std::function<std::unique_ptr<CompressionAlgorithm>()>;
const std::vector<Factory> industries {
    [] { return std::make_unique<Huffman>(); }, // 0
};

std::unique_ptr<CompressionAlgorithm> get_algorithm(int id) {
    if (id < 0 || id >= static_cast<int>(industries.size()))
        return nullptr;              // ou lançar exceção
    return industries[id]();
}

int main(int argc, char* argv[]) {

    cxxopts::Options options("InformationTheory", "\nPossibilitar Diferente Formas de Compactação para Arquivos\n");

    options.add_options()
        ("i,inputfile", "Inserir Nome ou Caminho do Arquivo de Entrada", cxxopts::value<std::string>())
        ("o,outputfile", "Inserir Nome ou Caminho do Arquivo de Saída", cxxopts::value<std::string>())
        ("n,number", "Insira o número correspondente ao algoritmo de compressão", cxxopts::value<int>())
        ("h,help", "Mostrar ajuda")
    ;

    auto result = options.parse(argc, argv);

    if(result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    if(!result.count("inputfile")) {
        std::cout << "Erro, deve inserir um arquivo para compactação" << std::endl;
        return 1;
    }
    std::string outputfilename {"a.out"};
    if(result.count("outputfile")) {
        outputfilename = result["outputfile"].as<std::string>();
    }
    int idx {0};
    if(result.count("number")) {
        idx = result["number"].as<int>();
    }

    // Criamos variáveis relativas ao arquivo de entrada e de saída
    File inputfile {result["inputfile"].as<std::string>(), true};
    File outputfile {outputfilename, false};
    File probably_inputfile {"probably_inputfile.txt", false};

    std::unique_ptr<CompressionAlgorithm> algorithm {get_algorithm(idx)};

    // Realizamos a compressão de teste
    std::vector<uint8_t> data = algorithm->apply(inputfile.read());
    outputfile.write(data);

    // Realizamos a descompressão de teste
    data = algorithm->deapply(data);
    probably_inputfile.write(data);

    return 0;
}

