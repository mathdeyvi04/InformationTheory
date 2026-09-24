#pragma once

#include "../CompressionAlgorithm.hpp"

class Huffman : public CompressionAlgorithm {
private:

    /**
     * @brief Define a origem das frequências utilizadas pelo algoritmo.
     */
    enum class Mode {
        DEFAULT,
        PERSONAL
    };

    /**
     * @brief Modo default de geração das frequências.
     */
    Mode m_mode {Mode::PERSONAL};

    /**
     * @brief Possível arquivo que conterá as frequências padrão.
     */
    std::ifstream m_possible_file_freq {};

    /**
     * @brief Frequência de cada um dos 256 possíveis valores de byte.
     *
     * O índice representa o byte e o valor representa sua frequência, a
     * qual pode ser bem alta, por isso colocamos em uint64_t.
     */
    std::array<uint64_t, 256> m_freq {};

    /**
     * @brief Representa um nó da árvore de Huffman.
     *
     * Nós folha possuem um byte válido e não possuem filhos.
     * Nós internos possuem apenas os índices dos filhos.
     */
    struct HuffmanNode {
        uint64_t freq {};
        uint8_t byte {};
        int left {-1};
        int right {-1};
    };

    /**
     * @brief Compara nós pela frequência para a priority_queue.
     *
     * Faz com que o nó de menor frequência fique no topo da fila.
     */
    struct NodeComparer {
        const std::vector<HuffmanNode>* nodes;

        bool operator()(int a, int b) const {
            // A priority_queue coloca o maior elemento no topo.
            // Aqui, desejamos o contrário.
            const HuffmanNode& node_a = (*nodes)[a];
            const HuffmanNode& node_b = (*nodes)[b];

            if(node_a.freq != node_b.freq) {
                return node_a.freq > node_b.freq;
            }

            return a > b;
        }
    };

    /**
     * @brief Armazena todos os nós da árvore de Huffman.
     */
    std::vector<HuffmanNode> m_nodes {};

    /**
     * @brief Representará o índice da raiz da árvore
     */
    int m_root {};

    /**
     * @brief Quantidade de símbolos, (bytes), diferentes utilizada pela árvore
     */
    uint16_t m_symbol_count {};

    /**
     * @brief Constrói a árvore de Huffman a partir das frequências.
     *
     * @return Índice do nó raiz ou -1 caso não existam dados.
     */
    int build_tree() {

        m_nodes.clear();
        std::priority_queue<
            int,
            std::vector<int>,
            NodeComparer
        > queue {NodeComparer{&m_nodes}};

        for(uint16_t byte = 0; byte < 256; ++byte) {

            // Se a frequência for zero, não precisamos inserir na árvore.
            if(!m_freq[byte]) {
                continue;
            }
            ++m_symbol_count;

            const int index = static_cast<int>(m_nodes.size());

            m_nodes.push_back(
                {
                    m_freq[byte],
                    static_cast<uint8_t>(byte)
                }
            );

            queue.push(index);
        }

        // Arquivo vazio
        if(queue.empty()) {
            return -1;
        }

        // Arquivo contendo apenas um símbolo.
        if(queue.size() == 1) {
            return queue.top();
        }

        /*
         * Enquanto houver mais de um nó na fila:
         *
         *     1. Pegamos o menor.
         *     2. Pegamos o segundo menor.
         *     3. Criamos um pai contendo os dois.
         *     4. Inserimos o pai novamente na fila.
         *
         * Ao final sobrará apenas a raiz.
         */
        while(queue.size() > 1) {

            const int left = queue.top();
            queue.pop();

            const int right = queue.top();
            queue.pop();

            const int parent = static_cast<int>(m_nodes.size());

            m_nodes.push_back(
                {
                    m_nodes[left].freq + m_nodes[right].freq,
                    0,
                    left,
                    right
                }
            );

            queue.push(parent);
        }

        return queue.top();
    }

    /**
     * @brief Gera o código Huffman de cada byte percorrendo a árvore.
     *
     * @param node Índice do nó atual.
     * @param code Código construído até o momento.
     * @param codes Tabela contendo o código de cada byte.
     */
    void generate_codes(
        int node,
        std::vector<bool>& code,
        std::array<std::vector<bool>, 256>& codes
    ) const {

        const HuffmanNode& current = m_nodes[node];

        // Nó folha: encontramos o código completo deste byte.
        if(current.left == -1 && current.right == -1) {

            /*
             * Caso especial:
             * se existir apenas um byte diferente no arquivo,
             * seu código seria vazio. Nesse caso utilizamos "0".
             */
            if(code.empty()) {
                code.push_back(false);
            }

            codes[current.byte] = code;
            return;
        }

        // Caminho para a esquerda representa o bit 0.
        if(current.left != -1) {

            code.push_back(false);

            generate_codes(
                current.left,
                code,
                codes
            );

            code.pop_back();
        }

        // Caminho para a direita representa o bit 1.
        if(current.right != -1) {

            code.push_back(true);

            generate_codes(
                current.right,
                code,
                codes
            );

            code.pop_back();
        }
    }

    // Little endian é uma convenção de ordem de bytes na memória:
    // o byte menos significativo (LSB, least significant byte) é
    // armazenado primeiro (no menor endereço)

    /**
     * @brief Adiciona um inteiro de 16 bits ao vetor em little-endian.
     *
     * @param data Vetor que receberá os bytes.
     * @param value Valor que será serializado.
     */
    static void append_uint16(
        std::vector<uint8_t>& data,
        uint16_t value
    ) {
        data.push_back(static_cast<uint8_t>(value));
        data.push_back(static_cast<uint8_t>(value >> 8));
    }

    /**
     * @brief Adiciona um inteiro de 64 bits ao vetor em little-endian.
     *
     * @param data Vetor que receberá os bytes.
     * @param value Valor que será serializado.
     */
    static void append_uint64(
        std::vector<uint8_t>& data,
        uint64_t value
    ) {
        for(int i = 0; i < 8; ++i) {
            data.push_back(
                static_cast<uint8_t>(value >> (i * 8))
            );
        }
    }

    /**
     * @brief Lê um inteiro de 16 bits em little-endian.
     *
     * @param data Vetor contendo os dados.
     * @param position Posição atual da leitura.
     * @param value Variável que receberá o valor lido.
     *
     * @return true caso a leitura seja válida.
     */
    static bool read_uint16(
        const std::vector<uint8_t>& data,
        size_t& position,
        uint16_t& value
    ) {
        if(position + 2 > data.size()) {
            return false;
        }

        value =
            static_cast<uint16_t>(data[position]) |
           (static_cast<uint16_t>(data[position + 1]) << 8);

        position += 2;

        return true;
    }

     /**
     * @brief Lê um inteiro de 64 bits em little-endian.
     *
     * @param data Vetor contendo os dados.
     * @param position Posição atual da leitura.
     * @param value Variável que receberá o valor lido.
     *
     * @return true caso a leitura seja válida.
     */
    static bool read_uint64(
        const std::vector<uint8_t>& data,
        size_t& position,
        uint64_t& value
    ) {
        if(position + 8 > data.size()) {
            return false;
        }

        value = 0;

        for(int i = 0; i < 8; ++i) {
            value |=
                static_cast<uint64_t>(data[position + i])
                << (i * 8);
        }

        position += 8;

        return true;
    }

public:

    /**
     * @brief Inicializa o algoritmo e tenta carregar as frequências padrão.
     */
    Huffman() {

        m_possible_file_freq.open(
            "./src/LossLessCompression/vector_freq.txt",
            std::ios::binary
        );

        if(!m_possible_file_freq.is_open()) {
            // Caso não exista, então continuamos no modo PERSONAL
            return;
        }

        m_mode = Mode::DEFAULT;

        for(int i = 0; i < 256; ++i) {

            if(!(m_possible_file_freq >> m_freq[i])) {
                // Observe que serão lidos as N linhas do arquivo.
                // Caso N < 256, ele assumirá o valor 0 para os demais 256 - N elementos.
                ++m_symbol_count;
                break;
            }
        }

        /*
         * Constrói a árvore utilizando o vetor de frequências
         * que já temos neste ponto.
         */
        m_root = build_tree();
    }

    /**
     * @brief Comprime os dados utilizando o algoritmo de Huffman.
     *
     * As frequências são utilizadas para construir a árvore.
     * Em seguida, cada byte recebe seu código Huffman e os bits
     * resultantes são compactados em bytes.
     *
     * @param data Dados originais que serão comprimidos.
     *
     * @return Dados comprimidos em formato binário.
     */
    std::vector<uint8_t> apply(
        const std::vector<uint8_t>& data
    ) override {

        /*
         * No modo PERSONAL, as frequências, árvore e raiz precisam ser calculadas
         * a partir dos dados recebidos.
         */
        if(m_mode == Mode::PERSONAL) {

            m_freq.fill(0);
            for(uint8_t byte : data) {
                ++m_freq[byte];
            }

            /*
             * A árvore pertence à execução atual.
             * Portanto, removemos os nós da execução anterior.
             */
            m_nodes.clear();

            /*
             * Constrói a árvore utilizando o vetor de frequências
             * que já temos neste ponto.
             */
            m_root = build_tree();

            if(m_root == -1) {

                if(m_mode == Mode::PERSONAL) {
                    m_freq.fill(0);
                }

                return {};
            }
        }

        /*
         * Para cada um dos 256 possíveis bytes, armazenaremos
         * seu respectivo código Huffman.
         */
        std::array<std::vector<bool>, 256> codes {};

        std::vector<bool> current_code;

        /*
         * Percorre a árvore a partir da raiz para descobrir
         * o código Huffman de cada símbolo.
         */
        generate_codes(
            m_root,
            current_code,
            codes
        );

        /*
         * Aqui finalmente aplicamos os códigos aos dados originais.
         *
         * Os bits são acumulados em current_byte.
         * Quando tivermos 8 bits, ele é colocado no vetor de saída.
         */
        std::vector<uint8_t> compressed;

        /*
         * O cabeçalho possui:
         *
         * 2 bytes -> magic
         * 1 byte  -> versão
         * 2 bytes -> quantidade de símbolos
         * 8 bytes -> tamanho original
         *
         * Cada símbolo ocupa:
         *
         * 1 byte  -> símbolo
         * 8 bytes -> frequência
         */
        compressed.reserve(
            13 +
            static_cast<size_t>(m_symbol_count) * 9 +
            data.size()
        );

        // Magic: "HF"
        compressed.push_back('H');
        compressed.push_back('F');

        // Versão do formato.
        compressed.push_back(1);

        // Quantidade de símbolos presentes na tabela.
        Huffman::append_uint16(compressed, m_symbol_count);

        // Tamanho original é necessário para eliminar o padding.
        Huffman::append_uint64(
            compressed,
            static_cast<uint64_t>(data.size())
        );

        if(data.empty()) {
            return compressed;
        }

        /*
         * Armazena a tabela de frequências no cabeçalho do arquivo.
         */
        for(uint16_t byte = 0; byte < 256; ++byte) {

            if(!m_freq[byte]) {
                continue;
            }

            compressed.push_back(
                static_cast<uint8_t>(byte)
            );

            append_uint64(
                compressed,
                m_freq[byte]
            );
        }

        uint8_t current_byte = 0;
        uint8_t bit_count = 0;

        for(uint8_t byte : data) {

            const std::vector<bool>& code = codes[byte];

            for(bool bit : code) {

                /*
                 * Os bits são armazenados do mais significativo
                 * para o menos significativo:
                 *
                 * bit = 1
                 * current_byte = current_byte << 1 | 1
                 */
                current_byte <<= 1;

                if(bit) {
                    current_byte |= 1;
                }

                ++bit_count;

                /*
                 * Um byte foi completamente preenchido.
                 */
                if(bit_count == 8) {

                    compressed.push_back(current_byte);

                    current_byte = 0;
                    bit_count = 0;
                }
            }
        }

        /*
         * Caso tenham sobrado bits que não completaram um byte,
         * deslocamos para a esquerda para preencher os bits
         * restantes com zeros.
         */
        if(bit_count > 0) {

            current_byte <<= (8 - bit_count);

            compressed.push_back(current_byte);
        }

        return compressed;
    }

    /**
     * @brief Descomprime dados produzidos por apply().
     *
     * Lê o cabeçalho, reconstrói a mesma árvore de Huffman
     * utilizada na compressão e percorre os bits comprimidos até
     * recuperar a quantidade original de bytes.
     *
     * @param data Dados comprimidos produzidos por apply().
     *
     * @return Dados originais descomprimidos ou vetor vazio em caso
     * de entrada inválida.
     */
    std::vector<uint8_t> deapply(
        const std::vector<uint8_t>& data
    ) override {

        /*
         * O cabeçalho mínimo contém:
         *
         * 2 bytes -> magic
         * 1 byte  -> versão
         * 2 bytes -> quantidade de símbolos
         * 8 bytes -> tamanho original
         */
        if(data.size() < 13) {
            return {};
        }

        size_t position = 0;

        // Verifica o identificador do formato.
        if(data[position++] != 'H' ||
           data[position++] != 'F') {
            return {};
        }

        // Verifica a versão do formato.
        const uint8_t version = data[position++];

        if(version != 1) {
            return {};
        }

        m_symbol_count = 0;
        if(!read_uint16(
            data,
            position,
            m_symbol_count
        )) {
            return {};
        }

        /*
         * Um arquivo não vazio precisa possuir pelo menos
         * um símbolo na árvore. E também há um limite de 256 símbolos.
         */
        if(m_symbol_count == 0 || m_symbol_count > 256) {
            return {};
        }

        uint64_t original_size = 0;

        if(!read_uint64(
            data,
            position,
            original_size
        )) {
            return {};
        }

        /*
         * Arquivo original vazio.
         */
        if(original_size == 0) {
            return {};
        }

        // Limpamos toda a estrutura.
        m_freq.fill(0);

        /*
         * Reconstrói a tabela de frequências que originou a árvore.
         */
        for(uint16_t i = 0; i < m_symbol_count; ++i) {

            if(position >= data.size()) {
                return {};
            }

            const uint8_t byte = data[position++];

            uint64_t frequency = 0;

            if(!read_uint64(
                data,
                position,
                frequency
            )) {
                return {};
            }

            /*
             * Frequência zero não faz sentido no cabeçalho.
             */
            if(frequency == 0) {
                return {};
            }

            m_freq[byte] = frequency;
        }

        /*
         * Reconstrói exatamente a árvore utilizada na compressão.
         *
         * O desempate determinístico do NodeComparer é importante
         * para que a mesma tabela produza a mesma árvore.
         */
        m_root = build_tree();

        if(m_root == -1) {
            return {};
        }

        /*
         * Reserva aproximadamente o tamanho esperado da saída.
         */
        std::vector<uint8_t> decompressed;

        decompressed.reserve(
            static_cast<size_t>(original_size)
        );

        const HuffmanNode& root_node = m_nodes[m_root];

        /*
         * Caso especial:
         *
         * Se existe somente um símbolo, seu código é "0".
         * Portanto não precisamos sequer percorrer os bits.
         */
        if(
            root_node.left  == -1 &&
            root_node.right == -1
        ) {

            for(uint64_t i = 0; i < original_size; ++i) {
                decompressed.push_back(root_node.byte);
            }

            return decompressed;
        }

        /*
         * Começamos na raiz e descemos pela árvore conforme
         * os bits são lidos.
         */
        int current_node {m_root};

        for(size_t i = position; i < data.size(); ++i) {

            const uint8_t current_byte = data[i];

            /*
             * Os bits são lidos do mais significativo para
             * o menos significativo, exatamente na ordem em
             * que foram gravados por apply().
             */
            for(int bit = 7; bit >= 0; --bit) {

                const bool value = (current_byte & (1u << bit)) != 0;

                current_node = value ? m_nodes[current_node].right
                                     : m_nodes[current_node].left;

                /*
                 * Um caminho inválido indica dados corrompidos.
                 */
                if(current_node == -1) {
                    return {};
                }

                const HuffmanNode& node = m_nodes[current_node];

                /*
                 * Chegamos a uma folha:
                 * um símbolo foi recuperado.
                 */
                if(
                    node.left == -1 &&
                    node.right == -1
                ) {

                    decompressed.push_back(node.byte);

                    /*
                     * Já recuperamos exatamente o tamanho original.
                     *
                     * Os bits restantes do último byte são apenas
                     * padding e devem ser ignorados.
                     */
                    if(
                        decompressed.size() ==
                        static_cast<size_t>(original_size)
                    ) {
                        return decompressed;
                    }

                    // Começamos o próximo símbolo novamente na raiz.
                    current_node = m_root;
                }
            }
        }

        /*
         * Chegamos ao fim dos dados sem recuperar todos os bytes
         * esperados. Isso indica que os dados estão incompletos.
         */
        return {};
    }
};