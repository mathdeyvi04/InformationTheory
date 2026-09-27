#pragma once

#include "../core/CompressionAlgorithm.hpp"

class Huffman : public CompressionAlgorithm {
private:

    /** @brief Valor reservado para representar um filho inexistente. */
    static constexpr uint16_t INVALID_NODE {0xFFFF};

    /** @brief Quantidade Máxima de Bytes Possíveis. */
    static constexpr size_t MAX_POSSIBLE_BYTES {256};

    /** @brief Quantidade máxima de nós em uma árvore com 256 símbolos. */
    static constexpr uint16_t MAX_TREE_NODES {2 * MAX_POSSIBLE_BYTES - 1};

    /** @brief Quantidade máxima de bits necessária para armazenar todos os códigos. */
    static constexpr uint16_t MAX_CODE_BITS {MAX_POSSIBLE_BYTES * (MAX_POSSIBLE_BYTES - 1) / 2};

    /** @brief Quantidade máxima de bytes necessária para armazenar todos os códigos. */
    static constexpr uint16_t MAX_CODE_BYTES {(MAX_CODE_BITS + 7) / 8};

    /** @brief Versão do formato binário utilizado pelo Huffman. */
    static constexpr uint8_t FORMAT_VERSION {2};

    /** @brief Tamanho fixo do cabeçalho em bytes. */
    static constexpr size_t HEADER_SIZE {13};

    /**
     * @brief Quantitativo de símbolos (bytes) presente no arquivo
     */
    uint16_t m_symbol_count {};

    /**
     * @brief Representa um nó da árvore de Huffman.
     *
     * A árvore não armazena mais as frequências depois de construída.
     * Os filhos são identificados por índices de 16 bits.
     */
    struct HuffmanNode {
        uint16_t left  {INVALID_NODE};
        uint16_t right {INVALID_NODE};
        uint8_t byte   {};
    };

    /**
     * @brief Armazena os nós da árvore em memória contígua.
     */
    std::array<HuffmanNode, MAX_TREE_NODES> m_nodes {};

    /**
     * @brief Quantidade de nós atualmente utilizados.
     */
    uint16_t m_node_count {};

    /**
     * @brief Descreve a localização de um código no buffer de bits.
     */
    struct CodeInfo {
        uint16_t offset {};
        uint8_t  length {};
    };

    /**
     * @brief Verifica se um nó é folha.
     *
     * @param node Nó que será analisado.
     *
     * @return true caso seja folha.
     */
    static bool is_leaf(
        const HuffmanNode& node
    ) {
        return
            node.left == INVALID_NODE &&
            node.right == INVALID_NODE;
    }

    /**
     * @brief Constrói a árvore de Huffman a partir de histogram.
     *
     * A priority_queue armazena a frequência separadamente da árvore,
     * evitando manter uint64_t dentro de cada nó.
     *
     * @return Índice da raiz ou INVALID_NODE caso não existam símbolos.
     */
    uint16_t build_tree(const std::array<uint64_t, MAX_POSSIBLE_BYTES>& histogram, const std::set<uint8_t>& byte_set) {

        m_node_count = 0;

        using QueueEntry = std::pair<uint64_t, uint16_t>;

        /*
         * std::greater<> faz com que o menor par fique no topo.
         *
         * O primeiro elemento do par é a frequência.
         * O segundo é o índice do nó e serve como desempate determinístico.
         */
        std::priority_queue<
            QueueEntry,
            std::vector<QueueEntry>,
            std::greater<>
        > queue;

        for(const auto& byte : byte_set) {

            if(m_node_count >= MAX_TREE_NODES) {
                return INVALID_NODE;
            }

            const uint16_t index = m_node_count++;

            m_nodes[index] = {
                INVALID_NODE,
                INVALID_NODE,
                static_cast<uint8_t>(byte)
            };

            queue.push({
                histogram[byte],
                index
            });
        }

        if(queue.empty()) {
            return INVALID_NODE;
        }

        /*
         * Uma árvore contendo apenas um símbolo possui
         * somente uma folha, que será a própria raiz.
         */
        if(queue.size() == 1) {
            return queue.top().second;
        }

        /*
         * A cada iteração:
         *
         * 1. Retiramos o menor nó.
         * 2. Retiramos o segundo menor.
         * 3. Criamos o pai dos dois.
         * 4. Inserimos o pai novamente na fila.
         */
        while(queue.size() > 1) {

            const QueueEntry left = queue.top();
            queue.pop();

            const QueueEntry right = queue.top();
            queue.pop();

            if(m_node_count >= MAX_TREE_NODES) {
                return INVALID_NODE;
            }

            const uint16_t parent = m_node_count++;

            m_nodes[parent] = {
                left.second,
                right.second,
                0
            };

            queue.push({
                left.first + right.first,
                parent
            });
        }

        return queue.top().second;
    }

    /**
     * @brief Define um bit no armazenamento compacto dos códigos.
     *
     * @param bits Buffer contendo os códigos.
     * @param position Posição do bit.
     * @param value Valor do bit.
     */
    static void set_code_bit(
        std::array<uint8_t, MAX_CODE_BYTES>& bits,
        uint16_t position,
        bool value
    ) {

        if(value) {

            bits[position / 8] |=
                static_cast<uint8_t>(
                    1u << (7 - position % 8)
                );
        }
    }

    /**
     * @brief Lê um bit do armazenamento compacto dos códigos.
     *
     * @param bits Buffer contendo os códigos.
     * @param position Posição do bit.
     *
     * @return Valor do bit.
     */
    static bool get_code_bit(
        const std::array<uint8_t, MAX_CODE_BYTES>& bits,
        uint16_t position
    ) {

        return (
            bits[position / 8] &
            static_cast<uint8_t>(
                1u << (7 - position % 8)
            )
        ) != 0;
    }

    /**
     * @brief Gera os códigos Huffman dos símbolos da árvore.
     *
     * Os códigos são armazenados em um único buffer de bits,
     * evitando 256 objetos std::vector<bool>.
     *
     * @param node Índice do nó atual.
     * @param current_code Caminho atual na árvore.
     * @param current_length Tamanho do caminho atual.
     * @param codes Tabela de lookup dos códigos.
     * @param code_bits Buffer compacto contendo todos os códigos.
     * @param code_bit_count Quantidade de bits já utilizados no buffer.
     *
     * @return true caso a geração tenha sido concluída com sucesso.
     */
    bool generate_codes(
        uint16_t node,
        std::array<uint8_t, MAX_POSSIBLE_BYTES>& current_code,
        uint16_t current_length,
        std::array<CodeInfo, MAX_POSSIBLE_BYTES>& codes,
        std::array<uint8_t, MAX_CODE_BYTES>& code_bits,
        uint16_t& code_bit_count
    ) const {

        const HuffmanNode& current = m_nodes[node];

        /*
         * Encontramos um símbolo.
         */
        if(is_leaf(current)) {

            /*
             * Uma árvore com somente um símbolo teria
             * um código vazio. Utilizamos 0 nesse caso.
             */
            if(current_length == 0) {

                current_code[0] = 0;
                current_length = 1;
            }

            if(
                current_length > MAX_CODE_BITS ||
                code_bit_count >
                    MAX_CODE_BITS - current_length
            ) {
                return false;
            }

            const uint16_t offset = code_bit_count;

            codes[current.byte] = {
                offset,
                static_cast<uint8_t>(current_length)
            };

            /*
             * Copia o código atual para o buffer compacto.
             */
            for(uint16_t i = 0; i < current_length; ++i) {

                set_code_bit(
                    code_bits,
                    code_bit_count++,
                    current_code[i] != 0
                );
            }

            return true;
        }

        /*
         * Esquerda representa o bit 0.
         */
        if(current.left != INVALID_NODE) {

            current_code[current_length] = 0;

            if(!generate_codes(
                current.left,
                current_code,
                static_cast<uint16_t>(current_length + 1),
                codes,
                code_bits,
                code_bit_count
            )) {
                return false;
            }
        }

        /*
         * Direita representa o bit 1.
         */
        if(current.right != INVALID_NODE) {

            current_code[current_length] = 1;

            if(!generate_codes(
                current.right,
                current_code,
                static_cast<uint16_t>(current_length + 1),
                codes,
                code_bits,
                code_bit_count
            )) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Adiciona um bit ao fluxo serializado da árvore.
     *
     * @param data Vetor que contém a árvore serializada.
     * @param bit_position Posição do próximo bit.
     * @param value Valor do bit.
     */
    static void append_tree_bit(
        std::vector<uint8_t>& data,
        size_t& bit_position,
        bool value
    ) {

        /*
         * Um novo byte é criado a cada 8 bits.
         */
        if(bit_position % 8 == 0) {
            data.push_back(0);
        }

        if(value) {

            data.back() |=
                static_cast<uint8_t>(
                    1u << (7 - bit_position % 8)
                );
        }

        ++bit_position;
    }

    /**
     * @brief Serializa a árvore em pré-ordem.
     *
     * Nó interno = 0
     * Nó folha   = 1 + byte
     *
     * @param node Índice do nó atual.
     * @param data Vetor que receberá a árvore.
     * @param bit_position Posição atual dentro da serialização.
     */
    void serialize_tree(
        uint16_t node,
        std::vector<uint8_t>& data,
        size_t& bit_position
    ) const {

        const HuffmanNode& current = m_nodes[node];

        /*
         * Folha:
         *
         * 1 bit  -> marcador 1
         * 8 bits -> byte armazenado
         */
        if(is_leaf(current)) {

            append_tree_bit(
                data,
                bit_position,
                true
            );

            for(int bit = 7; bit >= 0; --bit) {

                append_tree_bit(
                    data,
                    bit_position,
                    (current.byte & (1u << bit)) != 0
                );
            }

            return;
        }

        /*
         * Nó interno:
         *
         * 1 bit -> marcador 0
         */
        append_tree_bit(
            data,
            bit_position,
            false
        );

        serialize_tree(
            current.left,
            data,
            bit_position
        );

        serialize_tree(
            current.right,
            data,
            bit_position
        );
    }

    /**
     * @brief Adiciona um inteiro de 16 bits em little-endian.
     *
     * @param data Vetor que receberá os bytes.
     * @param value Valor a ser serializado.
     */
    static void append_uint16(
        std::vector<uint8_t>& data,
        uint16_t value
    ) {

        data.push_back(
            static_cast<uint8_t>(value)
        );

        data.push_back(
            static_cast<uint8_t>(value >> 8)
        );
    }

    /**
     * @brief Adiciona um inteiro de 64 bits em little-endian.
     *
     * @param data Vetor que receberá os bytes.
     * @param value Valor a ser serializado.
     */
    static void append_uint64(
        std::vector<uint8_t>& data,
        uint64_t value
    ) {

        for(int i = 0; i < 8; ++i) {

            data.push_back(
                static_cast<uint8_t>(
                    value >> (i * 8)
                )
            );
        }
    }

    /**
     * @brief Lê um inteiro de 16 bits em little-endian.
     *
     * @param data Dados de entrada.
     * @param position Posição atual da leitura.
     * @param value Variável que receberá o valor.
     *
     * @return true caso existam bytes suficientes.
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
            (
                static_cast<uint16_t>(
                    data[position + 1]
                ) << 8
            );

        position += 2;

        return true;
    }

    /**
     * @brief Lê um inteiro de 64 bits em little-endian.
     *
     * @param data Dados de entrada.
     * @param position Posição atual da leitura.
     * @param value Variável que receberá o valor.
     *
     * @return true caso existam bytes suficientes.
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
                static_cast<uint64_t>(
                    data[position + i]
                ) << (i * 8);
        }

        position += 8;

        return true;
    }

    /**
     * @brief Lê um bit da árvore serializada.
     *
     * @param data Dados contendo a árvore.
     * @param tree_start Offset onde começa a árvore.
     * @param tree_bit_count Quantidade válida de bits da árvore.
     * @param bit_position Posição atual da leitura.
     * @param value Variável que receberá o bit.
     *
     * @return true caso a leitura seja válida.
     */
    static bool read_tree_bit(
        const std::vector<uint8_t>& data,
        size_t tree_start,
        uint16_t tree_bit_count,
        uint16_t& bit_position,
        bool& value
    ) {

        if(bit_position >= tree_bit_count) {
            return false;
        }

        const size_t byte_position =
            tree_start + bit_position / 8;

        if(byte_position >= data.size()) {
            return false;
        }

        value =
            (
                data[byte_position] &
                static_cast<uint8_t>(
                    1u << (7 - bit_position % 8)
                )
            ) != 0;

        ++bit_position;

        return true;
    }

    /**
     * @brief Reconstrói a árvore a partir de sua representação binária.
     *
     * A representação utiliza pré-ordem:
     *
     * 0       -> nó interno
     * 1 + byte -> folha
     *
     * @param data Dados contendo a árvore serializada.
     * @param tree_start Offset onde começa a árvore.
     * @param tree_bit_count Quantidade de bits da árvore.
     * @param bit_position Posição atual da leitura.
     * @param seen_bytes Símbolos já encontrados.
     * @param leaf_count Quantidade de folhas encontradas.
     *
     * @return Índice do nó reconstruído ou INVALID_NODE em caso de erro.
     */
    uint16_t deserialize_tree(
        const std::vector<uint8_t>& data,
        size_t tree_start,
        uint16_t tree_bit_count,
        uint16_t& bit_position,
        std::array<bool, MAX_POSSIBLE_BYTES>& seen_bytes,
        uint16_t& leaf_count
    ) {

        bool leaf_marker = false;

        if(!read_tree_bit(
            data,
            tree_start,
            tree_bit_count,
            bit_position,
            leaf_marker
        )) {
            return INVALID_NODE;
        }

        /*
         * Folha.
         */
        if(leaf_marker) {

            uint8_t byte = 0;

            for(int bit = 7; bit >= 0; --bit) {

                bool value = false;

                if(!read_tree_bit(
                    data,
                    tree_start,
                    tree_bit_count,
                    bit_position,
                    value
                )) {
                    return INVALID_NODE;
                }

                if(value) {
                    byte |= static_cast<uint8_t>(1u << bit);
                }
            }

            /*
             * Um mesmo byte não pode aparecer duas vezes
             * como folha da árvore.
             */
            if(seen_bytes[byte]) {
                return INVALID_NODE;
            }

            if(leaf_count >= MAX_POSSIBLE_BYTES) {
                return INVALID_NODE;
            }

            if(m_node_count >= MAX_TREE_NODES) {
                return INVALID_NODE;
            }

            const uint16_t node = m_node_count++;

            m_nodes[node] = {
                INVALID_NODE,
                INVALID_NODE,
                byte
            };

            seen_bytes[byte] = true;
            ++leaf_count;

            return node;
        }

        /*
         * Nó interno.
         */
        if(m_node_count >= MAX_TREE_NODES) {
            return INVALID_NODE;
        }

        const uint16_t node = m_node_count++;

        const uint16_t left = deserialize_tree(
            data,
            tree_start,
            tree_bit_count,
            bit_position,
            seen_bytes,
            leaf_count
        );

        if(left == INVALID_NODE) {
            return INVALID_NODE;
        }

        const uint16_t right = deserialize_tree(
            data,
            tree_start,
            tree_bit_count,
            bit_position,
            seen_bytes,
            leaf_count
        );

        if(right == INVALID_NODE) {
            return INVALID_NODE;
        }

        m_nodes[node] = {
            left,
            right,
            0
        };

        return node;
    }

public:

    /**
     * @brief Comprime os dados utilizando Huffman.
     *
     * O formato produzido contém:
     *
     * 2 bytes -> magic "HF"
     * 1 byte  -> versão
     * 2 bytes -> quantidade de bits da árvore
     * 8 bytes -> tamanho original
     * N bytes -> árvore serializada
     * N bytes -> payload Huffman
     *
     * @param data Dados originais.
     *
     * @return Dados comprimidos.
     */
    std::vector<uint8_t> apply(
        const std::vector<uint8_t>& data
    ) override {

        /*
         * O caso vazio possui somente o header.
         */
        if(data.empty()) {

            std::vector<uint8_t> compressed;

            compressed.reserve(HEADER_SIZE);

            compressed.push_back('H');
            compressed.push_back('F');
            compressed.push_back(FORMAT_VERSION);

            append_uint16(
                compressed,
                0
            );

            append_uint64(
                compressed,
                0
            );

            return compressed;
        }

        /**
         * @brief Frequência de cada um dos 256 possíveis bytes.
         *
         * O índice representa o byte e o valor representa sua frequência.
         * A frequência pode ser grande, por isso utilizamos uint64_t.
         */
        std::array<uint64_t, MAX_POSSIBLE_BYTES> histogram {};

        /**
         * @brief Conjunto de Bytes utilizados
         */
        std::set<uint8_t> byte_set {};

        /*
         * Populamos o array de frequências de bytes
         */
        for(uint8_t byte : data) {
            ++histogram[byte];
            byte_set.insert(byte);
        }
        m_symbol_count = byte_set.size();

        /*
         * Constrói a árvore usando as frequências padrão.
         * Dentro da função também conseguimos obter o valor de m_symbol_count
         */
        const uint16_t root = build_tree(histogram, byte_set);

        /*
         * Caso especial em que existe somente um símbolo.
         *
         * Nesse caso, a árvore + tamanho original já são
         * suficientes para reconstruir todos os dados.
         */
        if(is_leaf(m_nodes[root])) {

            for(uint8_t byte : data) {

                if(byte != m_nodes[root].byte) {
                    return {};
                }
            }

            const uint16_t tree_bit_count =
                static_cast<uint16_t>(
                    m_symbol_count * 10 - 1
                );

            const size_t tree_byte_count =
                tree_bit_count / 8 +
                (tree_bit_count % 8 != 0 ? 1 : 0);

            std::vector<uint8_t> compressed;

            compressed.reserve(
                HEADER_SIZE + tree_byte_count
            );

            compressed.push_back('H');
            compressed.push_back('F');
            compressed.push_back(FORMAT_VERSION);

            append_uint16(
                compressed,
                tree_bit_count
            );

            append_uint64(
                compressed,
                static_cast<uint64_t>(data.size())
            );

            size_t tree_bits_written = 0;

            serialize_tree(
                root,
                compressed,
                tree_bits_written
            );

            if(
                tree_bits_written !=
                tree_bit_count
            ) {
                return {};
            }

            return compressed;
        }

        /*
         * Gera os códigos dos símbolos.
         */
        std::array<CodeInfo, MAX_POSSIBLE_BYTES> codes {};

        std::array<uint8_t, MAX_POSSIBLE_BYTES> current_code {};

        std::array<uint8_t, MAX_CODE_BYTES> code_bits {};

        uint16_t code_bit_count = 0;

        if(!generate_codes(
            root,
            current_code,
            0,
            codes,
            code_bits,
            code_bit_count
        )) {
            return {};
        }

        /*
         * Calcula antecipadamente o tamanho exato
         * do payload para realizar somente uma alocação.
         */
        size_t payload_bits = 0;

        for(const auto& byte : byte_set) {

            const CodeInfo code = codes[byte];
            payload_bits += code.length;
        }

        const size_t payload_byte_count =
            payload_bits / 8 +
            (payload_bits % 8 != 0 ? 1 : 0);

        const uint16_t tree_bit_count =
            static_cast<uint16_t>(
                m_symbol_count * 10 - 1
            );

        const size_t tree_byte_count =
            tree_bit_count / 8 +
            (tree_bit_count % 8 != 0 ? 1 : 0);

        /*
         * Agora que sabemos exatamente quanto será necessário,
         * realizamos uma única reserva para o arquivo.
         */
        std::vector<uint8_t> compressed;

        compressed.reserve(
            HEADER_SIZE +
            tree_byte_count +
            payload_byte_count
        );

        /*
         * Header.
         */
        compressed.push_back('H');
        compressed.push_back('F');
        compressed.push_back(FORMAT_VERSION);

        append_uint16(
            compressed,
            tree_bit_count
        );

        append_uint64(
            compressed,
            static_cast<uint64_t>(data.size())
        );

        /*
         * Serializa a árvore.
         */
        size_t tree_bits_written = 0;

        serialize_tree(
            root,
            compressed,
            tree_bits_written
        );

        if(
            tree_bits_written !=
            tree_bit_count
        ) {
            return {};
        }

        /*
         * Empacota os códigos Huffman em bytes.
         */
        uint8_t current_byte = 0;
        uint8_t bit_count = 0;

        for(uint8_t byte : data) {

            const CodeInfo code = codes[byte];

            for(uint16_t i = 0; i < code.length; ++i) {

                current_byte <<= 1;

                if(
                    get_code_bit(
                        code_bits,
                        static_cast<uint16_t>(
                            code.offset + i
                        )
                    )
                ) {
                    current_byte |= 1;
                }

                ++bit_count;

                if(bit_count == 8) {

                    compressed.push_back(
                        current_byte
                    );

                    current_byte = 0;
                    bit_count = 0;
                }
            }
        }

        /*
         * Preenche o último byte com zeros quando
         * a quantidade de bits não for múltipla de 8.
         */
        if(bit_count) {

            current_byte <<=
                static_cast<uint8_t>(
                    8 - bit_count
                );

            compressed.push_back(
                current_byte
            );
        }

        return compressed;
    }

    /**
     * @brief Descomprime os dados produzidos por apply().
     *
     * A função reconstrói a árvore armazenada no cabeçalho
     * e utiliza o payload para recuperar os bytes originais.
     *
     * @param data Dados comprimidos.
     *
     * @return Dados originais ou vetor vazio em caso de erro.
     */
    std::vector<uint8_t> deapply(
        const std::vector<uint8_t>& data
    ) override {

        if(data.size() < HEADER_SIZE) {
            return {};
        }

        size_t position = 0;

        /*
         * Magic.
         */
        if(
            data[position++] != 'H' ||
            data[position++] != 'F'
        ) {
            return {};
        }

        /*
         * Versão.
         */
        if(
            data[position++] !=
            FORMAT_VERSION
        ) {
            return {};
        }

        /*
         * Quantidade de bits ocupada pela árvore.
         */
        uint16_t tree_bit_count = 0;

        if(!read_uint16(
            data,
            position,
            tree_bit_count
        )) {
            return {};
        }

        /*
         * Tamanho original.
         */
        uint64_t original_size = 0;

        if(!read_uint64(
            data,
            position,
            original_size
        )) {
            return {};
        }

        /*
         * Arquivo vazio.
         */
        if(original_size == 0) {

            if(
                tree_bit_count != 0 ||
                data.size() != HEADER_SIZE
            ) {
                return {};
            }

            return {};
        }

        /*
         * Arquivo não vazio precisa possuir uma árvore.
         */
        if(tree_bit_count == 0) {
            return {};
        }

        /*
         * Verifica se o tamanho solicitado cabe em size_t.
         */
        if(
            original_size >
            std::numeric_limits<size_t>::max()
        ) {
            return {};
        }

        /*
         * Quantidade de bytes necessários para a árvore.
         */
        const size_t tree_byte_count =
            tree_bit_count / 8 +
            (tree_bit_count % 8 != 0 ? 1 : 0);

        if(
            (position + tree_byte_count) >
            data.size()
        ) {
            return {};
        }

        /*
         * A estrutura anterior da árvore deixa de ser válida.
         */
        m_node_count = 0;

        std::array<bool, MAX_POSSIBLE_BYTES> seen_bytes {};

        uint16_t leaf_count = 0;
        uint16_t tree_bit_position = 0;

        const size_t tree_start = position;

        /*
         * Reconstrói a árvore.
         */
        const uint16_t root = deserialize_tree(
            data,
            tree_start,
            tree_bit_count,
            tree_bit_position,
            seen_bytes,
            leaf_count
        );

        if(root == INVALID_NODE) {
            return {};
        }

        /*
         * A serialização precisa ter sido consumida
         * exatamente até o último bit da árvore.
         */
        if(
            tree_bit_position !=
            tree_bit_count
        ) {
            return {};
        }

        if(leaf_count == 0) {
            return {};
        }

        /*
         * Uma árvore binária completa com L folhas possui
         * exatamente 2L - 1 nós.
         */
        if(
            m_node_count !=
            static_cast<uint16_t>(
                leaf_count * 2 - 1
            )
        ) {
            return {};
        }

        /*
         * Para L folhas, a nossa serialização possui:
         *
         * L folhas × 9 bits
         * (L - 1) nós internos × 1 bit
         *
         * Total = 10L - 1 bits.
         */
        const uint16_t expected_tree_bits =
            static_cast<uint16_t>(
                leaf_count * 10 - 1
            );

        if(
            tree_bit_count !=
            expected_tree_bits
        ) {
            return {};
        }

        position += tree_byte_count;

        const HuffmanNode& root_node =
            m_nodes[root];

        /*
         * Caso especial: apenas um símbolo.
         *
         * Não existe payload nesse caso.
         */
        if(is_leaf(root_node)) {

            if(position != data.size()) {
                return {};
            }

            return std::vector<uint8_t>(
                static_cast<size_t>(original_size),
                root_node.byte
            );
        }

        /*
         * Uma árvore com múltiplos símbolos precisa
         * possuir payload.
         */
        if(position >= data.size()) {
            return {};
        }

        std::vector<uint8_t> decompressed;

        decompressed.reserve(
            static_cast<size_t>(original_size)
        );

        uint16_t current_node = root;

        /*
         * Percorre o payload bit a bit.
         */
        for(
            size_t i = position;
            i < data.size();
            ++i
        ) {

            const uint8_t current_byte =
                data[i];

            /*
             * Os bits são lidos do mais significativo
             * para o menos significativo.
             */
            for(int bit = 7; bit >= 0; --bit) {

                const bool value =
                    (
                        current_byte &
                        (1u << bit)
                    ) != 0;

                current_node = value
                    ? m_nodes[current_node].right
                    : m_nodes[current_node].left;

                /*
                 * Caminho inválido indica corrupção.
                 */
                if(
                    current_node ==
                    INVALID_NODE
                ) {
                    return {};
                }

                const HuffmanNode& node =
                    m_nodes[current_node];

                /*
                 * Chegamos a um símbolo.
                 */
                if(is_leaf(node)) {

                    decompressed.push_back(
                        node.byte
                    );

                    /*
                     * O tamanho original determina
                     * exatamente quando devemos parar.
                     *
                     * Os bits restantes do último byte
                     * são apenas padding.
                     */
                    if(
                        decompressed.size() ==
                        static_cast<size_t>(
                            original_size
                        )
                    ) {
                        return decompressed;
                    }

                    current_node = root;
                }
            }
        }

        /*
         * Terminamos o payload sem recuperar
         * todos os bytes esperados.
         */
        return {};
    }
};