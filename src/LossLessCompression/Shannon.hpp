#pragma once

#include "../core/CompressionAlgorithm.hpp"

class Shannon : public CompressionAlgorithm {
private:

    /** @brief Quantidade máxima de símbolos possíveis (universo de bytes). */
    static constexpr size_t MAX_POSSIBLE_BYTES {256};

    /** @brief Versão atual do formato Shannon-Elias. */
    static constexpr uint8_t FORMAT_VERSION {1};

    /** @brief Quantidade máxima de bits que um código pode ocupar. */
    static constexpr uint8_t MAX_CODE_BITS {65};

    /** @brief Quantidade máxima de bytes necessários para armazenar um código. */
    static constexpr size_t MAX_CODE_BYTES {
        (MAX_CODE_BITS + 7) / 8U
    };

    /**
     * @brief Tamanho fixo do header.
     *
     * Composto por:
     *  - 2 bytes  -> magic "SE"
     *  - 1 byte   -> versão do formato
     *  - 2 bytes  -> quantidade de símbolos
     *  - 8 bytes  -> tamanho original
     */
    static constexpr size_t FIXED_HEADER_SIZE {
        2 + // Magic
        1 + // Version
        2 + // Symbol count
        8   // Original size
    };

    /**
     * @brief Representa o código binário de um símbolo.
     *
     * Os bits são armazenados na ordem de transmissão
     * (do mais significativo para o menos significativo).
     */
    struct Code {
        /** @brief Buffer que armazena os bits do código. */
        std::array<uint8_t, MAX_CODE_BYTES> bytes {};

        /** @brief Quantidade efetiva de bits utilizados em @ref bytes. */
        uint8_t length {};
    };

    /**
     * @brief Compara símbolos pela frequência decrescente.
     *
     * Como P(x) = f(x) / N, ordenar por frequência é equivalente
     * a ordenar por probabilidade. Empates são desfeitos pelo
     * valor do próprio byte, garantindo determinismo.
     */
    struct FrequencyCompare {
        /** @brief Ponteiro para o histograma consultado nas comparações. */
        const std::array<uint64_t, MAX_POSSIBLE_BYTES>* histogram {};

        /**
         * @brief Compara dois símbolos.
         *
         * @param lhs Símbolo da esquerda.
         * @param rhs Símbolo da direita.
         *
         * @return true se @p lhs deve vir antes de @p rhs.
         */
        bool operator()(const uint8_t lhs, const uint8_t rhs) const {
            const uint64_t lhs_frequency {(*histogram)[lhs]};
            const uint64_t rhs_frequency {(*histogram)[rhs]};

            if(lhs_frequency == rhs_frequency) {
                return lhs > rhs;
            }
            return lhs_frequency < rhs_frequency;
        }
    };

    /**
     * @brief Preenche o histograma e coleta os símbolos presentes.
     *
     * Única varredura sobre as 256 posições do histograma. Os símbolos
     * presentes ficam em @p present[0..retorno), na ordem crescente de
     * byte, prontos para reuso nos laços seguintes.
     *
     * @param data      Dados originais.
     * @param histogram Array de saída com as frequências.
     * @param present   Array de saída com os símbolos presentes.
     *
     * @return Quantidade de símbolos distintos presentes nos dados.
     */
    static uint16_t build_histogram(
        const std::vector<uint8_t>& data,
        std::array<uint64_t, MAX_POSSIBLE_BYTES>& histogram,
        std::array<uint8_t, MAX_POSSIBLE_BYTES>& present
    ) {
        for(const uint8_t byte : data) {
            ++histogram[byte];
        }

        uint16_t count {0};

        for(size_t byte = 0; byte < MAX_POSSIBLE_BYTES; ++byte) {
            if(histogram[byte] != 0) {
                present[count++] = static_cast<uint8_t>(byte);
            }
        }

        return count;
    }

    /**
     * @brief Calcula o tamanho do código Shannon-Elias.
     *
     * Fórmula: ceil(-log2(P(x))) + 1.
     *
     * @param probability Probabilidade do símbolo (0 < P <= 1).
     *
     * @return Quantidade de bits do código.
     */
    static uint8_t get_code_length(const double probability) {
        return static_cast<uint8_t>(
            std::ceil(-std::log2(probability)) + 1.0
        );
    }

    /**
     * @brief Adiciona um bit ao final do código.
     *
     * @param code Código que receberá o bit.
     * @param bit  Valor do bit (0 ou 1).
     */
    static void append_code_bit(Code& code, const uint8_t bit) {
        const size_t byte_index {code.length / 8U};
        const uint8_t bit_index {
            static_cast<uint8_t>(code.length % 8)
        };
        const uint8_t shift {
            static_cast<uint8_t>(7 - bit_index)
        };

        code.bytes[byte_index] |=
            static_cast<uint8_t>(bit << shift);

        ++code.length;
    }

    /**
     * @brief Obtém um bit do código.
     *
     * O índice zero representa o primeiro bit do código.
     *
     * @param code      Código consultado.
     * @param bit_index Posição do bit (a partir de zero).
     *
     * @return Valor do bit (0 ou 1).
     */
    static uint8_t get_code_bit(
        const Code& code,
        const uint8_t bit_index
    ) {
        const size_t byte_index {bit_index / 8U};
        const uint8_t bit_offset {
            static_cast<uint8_t>(bit_index % 8)
        };
        const uint8_t shift {
            static_cast<uint8_t>(7 - bit_offset)
        };

        return static_cast<uint8_t>(
            (code.bytes[byte_index] >> shift) & 1
        );
    }

    /**
     * @brief Constrói o código a partir do ponto médio do intervalo.
     *
     * O ponto médio
     *
     *     F(x) + P(x) / 2
     *
     * pode ser representado exatamente por
     *
     *     (2 * cumulative_frequency + frequency) / (2 * total)
     *
     * de modo que a geração dos bits não depende da precisão do double.
     *
     * @param code                 Código de saída.
     * @param cumulative_frequency Soma das frequências dos símbolos anteriores.
     * @param frequency            Frequência do símbolo atual.
     * @param total                Quantidade total de bytes do arquivo.
     * @param length               Quantidade de bits a gerar.
     *
     * @return true em caso de sucesso.
     */
    static bool build_code(
        Code& code,
        const uint64_t cumulative_frequency,
        const uint64_t frequency,
        const uint64_t total,
        const uint8_t length
    ) {
        if(length == 0 || length > MAX_CODE_BITS) {
            return false;
        }

        /* Para manter as operações em uint64_t, 2 * total precisa caber. */
        if(total > std::numeric_limits<uint64_t>::max() / 2) {
            return false;
        }

        code = Code {};

        /*
         * Numerador do ponto médio: 2 * cumulative + frequency.
         * O denominador é 2 * total; usamos "total" como limiar.
         */
        uint64_t numerator {
            cumulative_frequency * 2 + frequency
        };

        for(uint8_t bit = 0; bit < length; ++bit) {
            const uint8_t current_bit {
                static_cast<uint8_t>(numerator >= total)
            };

            if(current_bit != 0) {
                /* 2 * numerator - 2 * total, escrito para evitar overflow. */
                numerator = (numerator - total) * 2;
            } else {
                numerator *= 2;
            }

            append_code_bit(code, current_bit);
        }

        return true;
    }

    /**
     * @brief Serializa um inteiro de 16 bits em little-endian.
     *
     * @param output Vetor que receberá os bytes.
     * @param value  Valor a ser serializado.
     */
    static void append_uint16(
        std::vector<uint8_t>& output,
        const uint16_t value
    ) {
        output.push_back(static_cast<uint8_t>(value & 0xFF));
        output.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    /**
     * @brief Serializa um inteiro de 64 bits em little-endian.
     *
     * @param output Vetor que receberá os bytes.
     * @param value  Valor a ser serializado.
     */
    static void append_uint64(
        std::vector<uint8_t>& output,
        const uint64_t value
    ) {
        for(uint8_t byte = 0; byte < sizeof(uint64_t); ++byte) {
            output.push_back(
                static_cast<uint8_t>(
                    (value >> (byte * 8)) & 0xFF
                )
            );
        }
    }

    /**
     * @brief Calcula a quantidade de bytes ocupados pelo código.
     *
     * @param code Código consultado.
     *
     * @return Quantidade de bytes (arredondada para cima).
     */
    static size_t get_code_byte_count(const Code& code) {
        return (static_cast<size_t>(code.length) + 7) / 8U;
    }

    /**
     * @brief Adiciona um código ao bitstream do payload.
     *
     * Aproveita o alinhamento do buffer para copiar bytes completos
     * de uma só vez quando possível.
     *
     * @param output               Vetor de saída (payload).
     * @param code                 Código a ser adicionado.
     * @param current_byte         Byte parcialmente preenchido em construção.
     * @param bits_in_current_byte Quantidade de bits já ocupados em @p current_byte.
     */
    static void append_code_to_payload(
        std::vector<uint8_t>& output,
        const Code& code,
        uint8_t& current_byte,
        uint8_t& bits_in_current_byte
    ) {
        const size_t full_bytes {code.length / 8U};

        /* Buffer alinhado: copia diretamente os bytes completos. */
        if(bits_in_current_byte == 0) {
            output.insert(
                output.end(),
                code.bytes.begin(),
                code.bytes.begin() + full_bytes
            );
        }
        else {
            for(size_t byte = 0; byte < full_bytes; ++byte) {
                for(uint8_t bit = 0; bit < 8; ++bit) {
                    current_byte = static_cast<uint8_t>(
                        (current_byte << 1) |
                        get_code_bit(
                            code,
                            static_cast<uint8_t>(byte * 8 + bit)
                        )
                    );

                    ++bits_in_current_byte;

                    if(bits_in_current_byte == 8) {
                        output.push_back(current_byte);
                        current_byte = 0;
                        bits_in_current_byte = 0;
                    }
                }
            }
        }

        /* Bits restantes do código (menos que um byte). */
        const uint8_t remaining_bits {
            static_cast<uint8_t>(code.length % 8)
        };
        const uint8_t first_remaining_bit {
            static_cast<uint8_t>(full_bytes * 8)
        };

        for(uint8_t bit = 0; bit < remaining_bits; ++bit) {
            current_byte = static_cast<uint8_t>(
                (current_byte << 1) |
                get_code_bit(
                    code,
                    static_cast<uint8_t>(first_remaining_bit + bit)
                )
            );

            ++bits_in_current_byte;

            if(bits_in_current_byte == 8) {
                output.push_back(current_byte);
                current_byte = 0;
                bits_in_current_byte = 0;
            }
        }
    }

    /*
     * A trie possui no máximo:
     *
     *   1 nó raiz +
     *   soma dos comprimentos dos códigos
     *
     * Como existem no máximo 256 símbolos e cada código possui
     * no máximo 65 bits, o índice de nó cabe em uint16_t.
     */
    struct TrieNode {
        std::array<uint16_t, 2> child {
            std::numeric_limits<uint16_t>::max(),
            std::numeric_limits<uint16_t>::max()
        };

        uint8_t symbol {0};
        uint8_t terminal {0};
    };

public:

    /**
     * @brief Comprime os dados utilizando Shannon-Fano-Elias.
     *
     * Layout produzido:
     *
     *  - 2 bytes -> magic "SE"
     *  - 1 byte  -> versão
     *  - 1 byte  -> flags
     *  - 2 bytes -> quantidade de símbolos
     *  - 8 bytes -> tamanho original
     *  - tabela  -> símbolos + comprimento + bytes do código
     *  - payload -> bitstream codificado
     *
     * @param data Dados originais.
     *
     * @return Dados comprimidos, ou vetor vazio em caso de erro.
     */
    std::vector<uint8_t> apply(
        const std::vector<uint8_t>& data
    ) override {

        if(data.empty()) {
            return {};
        }

        /*
         * Apenas dois arrays de 256 elementos persistem por chamada:
         *
         *   histogram -> 2048 bytes
         *   codes     -> ~2560 bytes
         *
         * present -> 256 bytes (índice compacto dos símbolos)
         *
         * histogram_prob foi eliminado: a probabilidade é calculada
         * inline na construção dos códigos (1 divisão por símbolo).
         */
        std::array<uint64_t, MAX_POSSIBLE_BYTES> histogram {};
        std::array<Code, MAX_POSSIBLE_BYTES> codes {};
        std::array<uint8_t, MAX_POSSIBLE_BYTES> present {};

        const uint16_t present_count {
            build_histogram(data, histogram, present)
        };

        const uint64_t total {
            static_cast<uint64_t>(data.size())
        };

        /* Priority queue com reserva exata do número de símbolos. */
        std::vector<uint8_t> queue_storage;
        queue_storage.reserve(present_count);

        std::priority_queue<
            uint8_t,
            std::vector<uint8_t>,
            FrequencyCompare
        > probability_queue {
            FrequencyCompare {&histogram},
            std::move(queue_storage)
        };

        for(uint16_t i = 0; i < present_count; ++i) {
            probability_queue.push(present[i]);
        }

        /*
         * Construção dos códigos.
         *
         * A probabilidade é calculada inline a partir de
         * frequency / total. Não armazenamos um array de double.
         */
        uint64_t cumulative_frequency {0};

        while(!probability_queue.empty()) {
            const uint8_t byte = probability_queue.top();
            probability_queue.pop();

            const uint64_t frequency = histogram[byte];
            const double probability =
                static_cast<double>(frequency) /
                static_cast<double>(total);

            const uint8_t length = get_code_length(probability);

            if(length == 0 || length > MAX_CODE_BITS) {
                return {};
            }

            if(!build_code(
                codes[byte],
                cumulative_frequency,
                frequency,
                total,
                length
            )) {
                return {};
            }

            cumulative_frequency += frequency;
        }

        /*
         * Payload bits e header size em uma ÚNICA varredura
         * sobre os símbolos presentes.
         */
        uint64_t payload_bits {0};
        size_t header_size {FIXED_HEADER_SIZE};

        for(uint16_t i = 0; i < present_count; ++i) {
            const uint8_t byte = present[i];
            const uint64_t frequency = histogram[byte];
            const uint64_t length = codes[byte].length;

            if(length != 0 &&
               frequency >
                   std::numeric_limits<uint64_t>::max() / length) {
                return {};
            }

            const uint64_t symbol_bits {frequency * length};

            if(payload_bits >
               std::numeric_limits<uint64_t>::max() -
                   symbol_bits) {
                return {};
            }

            payload_bits += symbol_bits;

            header_size += 2 + get_code_byte_count(codes[byte]);
        }

        const size_t payload_bytes {
            static_cast<size_t>((payload_bits + 7) / 8U)
        };

        if(header_size >
           std::numeric_limits<size_t>::max() - payload_bytes) {
            return {};
        }

        std::vector<uint8_t> compressed {};
        compressed.reserve(header_size + payload_bytes);

        /* ------------------------------- Header ------------------------------ */

        // Magic bytes: "SE" = Shannon-Elias.
        compressed.push_back('S');
        compressed.push_back('E');

        // Versão do formato.
        compressed.push_back(FORMAT_VERSION);

        // Quantidade de símbolos presentes.
        append_uint16(compressed, present_count);

        // Tamanho original do arquivo.
        append_uint64(compressed, total);

        /* --------------------------- Tabela de códigos ----------------------- */

        for(uint16_t i = 0; i < present_count; ++i) {
            const uint8_t byte = present[i];
            const Code& code = codes[byte];
            const size_t code_byte_count =
                get_code_byte_count(code);

            // Símbolo.
            compressed.push_back(byte);

            // Quantidade de bits do código.
            compressed.push_back(code.length);

            // Bytes do código (na ordem de transmissão).
            compressed.insert(
                compressed.end(),
                code.bytes.begin(),
                code.bytes.begin() + code_byte_count
            );
        }

        /* ------------------------------- Payload ----------------------------- */

        uint8_t current_byte {0};
        uint8_t bits_in_current_byte {0};

        for(const uint8_t byte : data) {
            append_code_to_payload(
                compressed,
                codes[byte],
                current_byte,
                bits_in_current_byte
            );
        }

        /* Padding do último byte parcialmente preenchido. */
        if(bits_in_current_byte != 0) {
            current_byte = static_cast<uint8_t>(
                current_byte << (8 - bits_in_current_byte)
            );
            compressed.push_back(current_byte);
        }

        return compressed;
    }

    /**
     * @brief Descomprime dados codificados com Shannon-Elias.
     *
     * Lê o header, recupera a tabela de códigos e percorre o
     * payload bit a bit até reconstruir exatamente o tamanho
     * original informado no header.
     *
     * @param data Dados comprimidos.
     *
     * @return Dados originais, ou vetor vazio em caso de erro.
     */
    std::vector<uint8_t> deapply(
        const std::vector<uint8_t>& data
    ) override {

        /*
         * O header fixo possui:
         *
         * 2 bytes -> Magic
         * 1 byte  -> Version
         * 2 bytes -> Symbol count
         * 8 bytes -> Original size
         */
        if(data.size() < FIXED_HEADER_SIZE) {
            return {};
        }

        size_t offset {0};

        /* ------------------------------- Magic ------------------------------- */

        if(data[offset++] != 'S' ||
           data[offset++] != 'E') {
            return {};
        }

        /* ------------------------------- Version ----------------------------- */

        const uint8_t version {
            data[offset++]
        };

        if(version != FORMAT_VERSION) {
            return {};
        }

        /* ---------------------------- Symbol count ---------------------------- */

        const uint16_t symbol_count {
            static_cast<uint16_t>(
                data[offset] |
                (static_cast<uint16_t>(data[offset + 1]) << 8U)
            )
        };

        offset += 2;

        if(symbol_count == 0 ||
           symbol_count > MAX_POSSIBLE_BYTES) {
            return {};
        }

        /* ----------------------------- Original size ------------------------- */

        uint64_t original_size {0};

        for(uint8_t byte = 0; byte < sizeof(uint64_t); ++byte) {

            original_size |=
                static_cast<uint64_t>(data[offset++]) <<
                (byte * 8U);
        }

        if(original_size == 0) {
            return {};
        }

        /*
         * std::vector utiliza size_t para representar quantidade
         * de elementos.
         */
        if(original_size >
           static_cast<uint64_t>(
               std::numeric_limits<size_t>::max()
           )) {
            return {};
        }

        /* --------------------------- Tabela de códigos ----------------------- */

        /*
         * O próprio índice do array representa o símbolo.
         *
         * codes[byte] contém o código daquele byte.
         *
         * length == 0 significa que o símbolo não está presente.
         */
        std::array<Code, MAX_POSSIBLE_BYTES> codes {};

        size_t total_code_bits {0};

        for(uint16_t symbol = 0;
            symbol < symbol_count;
            ++symbol) {

            if(data.size() - offset < 2U) {
                return {};
            }

            const uint8_t byte {
                data[offset++]
            };

            const uint8_t code_length {
                data[offset++]
            };

            /*
             * Um código válido precisa possuir pelo menos um bit
             * e não pode ultrapassar o limite definido pela classe.
             */
            if(code_length == 0 ||
               code_length > MAX_CODE_BITS) {
                return {};
            }

            /*
             * Não permitimos que o mesmo símbolo apareça duas vezes
             * na tabela.
             */
            if(codes[byte].length != 0) {
                return {};
            }

            const size_t code_byte_count {
                (static_cast<size_t>(code_length) + 7U) / 8U
            };

            if(data.size() - offset < code_byte_count) {
                return {};
            }

            codes[byte].length = code_length;

            for(size_t code_byte = 0;
                code_byte < code_byte_count;
                ++code_byte) {

                codes[byte].bytes[code_byte] =
                    data[offset++];
            }

            /*
             * Cada bit de um código pode representar, no pior caso,
             * um novo nó da trie.
             */
            if(total_code_bits >
               std::numeric_limits<size_t>::max() -
                   static_cast<size_t>(code_length)) {
                return {};
            }

            total_code_bits += static_cast<size_t>(code_length);
        }

        /*
         * Depois da tabela, offset aponta exatamente para o início
         * do payload.
         */
        if(offset >= data.size()) {
            return {};
        }

        /* -------------------------- Árvore de códigos ------------------------ */

        const uint16_t NO_NODE {
            std::numeric_limits<uint16_t>::max()
        };

        std::vector<TrieNode> trie {};

        if(total_code_bits >
           std::numeric_limits<size_t>::max() - 1U) {
            return {};
        }

        trie.reserve(1U + total_code_bits);
        trie.emplace_back();

        /*
         * Constrói a trie a partir dos códigos serializados.
         *
         * Além de acelerar a decodificação, esta etapa garante que a
         * tabela realmente representa códigos prefix-free:
         *
         *   - um código não pode ser prefixo de outro;
         *   - dois códigos não podem ser iguais.
         */
        for(size_t byte = 0;
            byte < MAX_POSSIBLE_BYTES;
            ++byte) {

            const Code& code {
                codes[byte]
            };

            if(code.length == 0) {
                continue;
            }

            uint16_t node_index {0};

            for(uint8_t bit_index = 0;
                bit_index < code.length;
                ++bit_index) {

                /*
                 * Se o nó atual já for terminal, existe um código
                 * anterior que é prefixo do código atual.
                 */
                if(trie[node_index].terminal != 0) {
                    return {};
                }

                const uint8_t bit {
                    get_code_bit(code, bit_index)
                };

                uint16_t next_node {
                    trie[node_index].child[bit]
                };

                if(next_node == NO_NODE) {

                    if(trie.size() >=
                       static_cast<size_t>(NO_NODE)) {
                        return {};
                    }

                    next_node =
                        static_cast<uint16_t>(trie.size());

                    trie[node_index].child[bit] = next_node;

                    trie.emplace_back();
                }

                node_index = next_node;
            }

            /*
             * Se já é terminal, o código atual é duplicado.
             */
            if(trie[node_index].terminal != 0) {
                return {};
            }

            /*
             * Se já possui filhos, o código atual é prefixo de
             * outro código existente.
             */
            if(trie[node_index].child[0] != NO_NODE ||
               trie[node_index].child[1] != NO_NODE) {
                return {};
            }

            trie[node_index].symbol =
                static_cast<uint8_t>(byte);

            trie[node_index].terminal = 1;
        }

        /* ------------------------------- Output ------------------------------- */

        std::vector<uint8_t> decompressed {};

        decompressed.reserve(
            static_cast<size_t>(original_size)
        );

        /* -------------------------- Decodificação ---------------------------- */

        /*
         * A decodificação agora percorre diretamente a trie.
         *
         * Cada bit realiza apenas:
         *
         *   nó atual -> filho 0/1
         *
         * Ao atingir um nó terminal, o símbolo é emitido e
         * a busca retorna à raiz.
         */
        uint16_t node_index {0};

        for(size_t payload_byte = offset;
            payload_byte < data.size() &&
            decompressed.size() <
                static_cast<size_t>(original_size);
            ++payload_byte) {

            const uint8_t input_byte {
                data[payload_byte]
            };

            for(uint8_t bit_position = 0;
                bit_position < 8U &&
                decompressed.size() <
                    static_cast<size_t>(original_size);
                ++bit_position) {

                const uint8_t bit {
                    static_cast<uint8_t>(
                        (input_byte >> (7U - bit_position)) & 1U
                    )
                };

                const uint16_t next_node {
                    trie[node_index].child[bit]
                };

                /*
                 * O caminho atual não pertence a nenhum código válido.
                 */
                if(next_node == NO_NODE) {
                    return {};
                }

                node_index = next_node;

                /*
                 * Encontramos um código completo.
                 */
                if(trie[node_index].terminal != 0) {

                    decompressed.push_back(
                        trie[node_index].symbol
                    );

                    node_index = 0;
                }
            }
        }

        /*
         * O tamanho original determina exatamente quantos símbolos
         * precisam ser reconstruídos.
         */
        if(decompressed.size() !=
           static_cast<size_t>(original_size)) {
            return {};
        }

        /*
         * Se ainda estivermos fora da raiz, o payload terminou no
         * meio de um código.
         */
        if(node_index != 0) {
            return {};
        }

        return decompressed;
    }
};