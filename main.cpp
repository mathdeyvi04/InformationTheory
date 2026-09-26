#include "./src/all_includes.hpp"

// Criamos um vetor com todas as possibilidades de algoritmos
using Factory = std::function<std::unique_ptr<CompressionAlgorithm>()>;
const std::vector<Factory> industries {
    [] { return std::make_unique<Huffman>(); }, // 0
    [] { return std::make_unique<Shannon>(); }, // 1
    [] { return std::make_unique<LZW>();     }, // 2
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
        ("n,number", "Insira o número correspondente ao algoritmo de compressão. (Default: 0)", cxxopts::value<int>())
        ("m,mode", "Modo de Operação, 0 - Comprimir, 1 - Descomprimir, 2 - Comprimir e Descomprimir. (Default: 0)", cxxopts::value<int>())
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
    std::string inputfilename {result["inputfile"].as<std::string>()};
    std::string outputfilename {"a.out"};
    if(result.count("outputfile")) {
        outputfilename = result["outputfile"].as<std::string>();
    }
    if(inputfilename == outputfilename) {
        std::cout << "Erro, arquivos de entrada e de saída não podem ter o mesmo caminho" << std::endl;
        return 1;
    }

    int idx {0};
    if(result.count("number")) {
        idx = result["number"].as<int>();
    }
    int mode {0};
    if(result.count("mode")) {
        mode = result["mode"].as<int>();
    }

    // Criamos variáveis relativas ao arquivo de entrada e de saída
    File inputfile  {inputfilename, true};
    std::vector<uint8_t> read_from_input = inputfile.read();

    std::unique_ptr<CompressionAlgorithm> algorithm {get_algorithm(idx)};

    if(mode == 2) {
        std::vector<uint8_t> data = algorithm->apply(read_from_input);
        File outputfile {outputfilename, false};
        outputfile.write(data);

        // Há garantia que a decompactação funcionará
        File probably_inputfile {"probably_inputfile.txt", false};
        data = algorithm->deapply(data);
        probably_inputfile.write(data);
        return 0;
    }

    std::vector<uint8_t> data = (mode == 0) ? algorithm->apply(read_from_input) : algorithm->deapply(read_from_input);
    if(read_from_input.size() != 0 && data.size() == 0) {
        // Esse erro é muito mais comum em situações de descompactação.
        std::cout << "Houve erro na descompactação. Talvez não seja um arquivo .HF" << std::endl;
        return 1;
    }
    File outputfile {outputfilename, false};
    outputfile.write(data);
    return 0;
}

