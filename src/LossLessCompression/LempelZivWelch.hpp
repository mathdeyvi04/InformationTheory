#pragma once

#include "../core/CompressionAlgorithm.hpp"

class LZW : public CompressionAlgorithm {
private:
    /// Primeiro código disponível para sequências criadas dinamicamente.
    static constexpr uint16_t FIRST_DYNAMIC_CODE {256};

    /// Quantidade máxima de códigos existentes no dicionário.
    static constexpr uint16_t MAX_DICTIONARY_SIZE {4096};

    /// Versão do formato do arquivo LZW.
    static constexpr uint8_t FORMAT_VERSION {1};

    /// Primeiro byte do identificador do formato LZW.
    static constexpr uint8_t MAGIC_0 {'L'};

    /// Segundo byte do identificador do formato LZW.
    static constexpr uint8_t MAGIC_1 {'W'};

    /// Tamanho fixo do cabeçalho em bytes.
    static constexpr size_t FIXED_HEADER_SIZE {
        2 + // Magic
        1 + // Version
        1 + // Bits válidos no último byte
        8   // Tamanho original
    };

    /*
     * A chave possui:
     *
     * 12 bits -> prefix_code
     * 8 bits  -> next_byte
     *
     * Total = 20 bits.
     */
    /// Quantidade de bits utilizada pela chave da tabela hash.
    static constexpr uint8_t HASH_KEY_BITS {20};

    /// Máscara dos 20 bits inferiores utilizados pela chave.
    static constexpr uint32_t HASH_KEY_MASK {
        (1U << HASH_KEY_BITS) - 1U
    };

    /*
     * A tabela utiliza:
     *
     * 20 bits -> chave
     * 12 bits -> código dinâmico
     *
     * Total = 32 bits.
     */
    /// Maior tamanho possível da tabela hash.
    static constexpr size_t MAX_HASH_TABLE_SIZE {8192};

    /**
     * Tabela usada para encontrar:
     *
     * (prefix_code + next_byte) -> dictionary_code
     *
     * A tabela é dimensionada uma única vez para cada execução.
     */
    struct Dictionary {
        /*
         * Cada posição armazena:
         *
         * bits  0..19 -> chave
         * bits 20..31 -> código do dicionário
         *
         * Valor 0 = posição vazia.
         *
         * Isso é seguro porque os códigos armazenados começam
         * em FIRST_DYNAMIC_CODE (256), portanto nunca podem
         * produzir um valor 0.
         */
        std::vector<uint32_t> entries;

        /// Máscara utilizada para calcular índices da tabela.
        size_t hash_mask {0};

        uint16_t next_code {FIRST_DYNAMIC_CODE};

        /**
         * Cria uma tabela com aproximadamente 50% de ocupação máxima.
         */
        explicit Dictionary(
            const size_t maximum_entries
        ) {
            const size_t target_size {
                maximum_entries * 2U
            };

            size_t table_size {1};

            while (
                table_size < target_size &&
                table_size < MAX_HASH_TABLE_SIZE
            ) {
                table_size <<= 1U;
            }

            entries.resize(table_size);

            hash_mask = table_size - 1U;
        }

        /**
         * Constrói uma chave única a partir do código anterior
         * e do próximo byte da entrada.
         */
        static uint32_t make_key(
            const uint16_t prefix_code,
            const uint8_t next_byte
        ) {
            return
                (static_cast<uint32_t>(prefix_code) << 8U) |
                static_cast<uint32_t>(next_byte);
        }

        /**
         * Hash rápido para a tabela.
         */
        static size_t hash_key(
            const uint32_t key,
            const size_t mask
        ) {
            uint32_t hash {key};

            hash ^= hash >> 16U;
            hash *= 0x7FEB352DU;
            hash ^= hash >> 15U;

            return static_cast<size_t>(hash) & mask;
        }

        /**
         * Procura uma sequência no dicionário.
         */
        bool find(
            const uint16_t prefix_code,
            const uint8_t next_byte,
            uint16_t& code
        ) const {
            const uint32_t key {
                make_key(prefix_code, next_byte)
            };

            size_t index {
                hash_key(key, hash_mask)
            };

            while (entries[index] != 0U) {
                const uint32_t entry {
                    entries[index]
                };

                if (
                    (entry & HASH_KEY_MASK) ==
                    key
                ) {
                    code = static_cast<uint16_t>(
                        entry >> HASH_KEY_BITS
                    );

                    return true;
                }

                index = (index + 1U) & hash_mask;
            }

            return false;
        }

        /**
         * Adiciona uma nova sequência ao dicionário.
         */
        void insert(
            const uint16_t prefix_code,
            const uint8_t next_byte
        ) {
            if (next_code >= MAX_DICTIONARY_SIZE) {
                return;
            }

            const uint32_t key {
                make_key(prefix_code, next_byte)
            };

            size_t index {
                hash_key(key, hash_mask)
            };

            while (entries[index] != 0U) {
                index = (index + 1U) & hash_mask;
            }

            entries[index] =
                (static_cast<uint32_t>(next_code) << HASH_KEY_BITS) |
                key;

            ++next_code;
        }
    };

    /**
     * Empacota códigos LZW em um fluxo contínuo de bits.
     */
    struct BitWriter {
        std::vector<uint8_t> data;

        uint64_t buffer {0};
        uint8_t bit_count {0};

        /**
         * Inicializa o arquivo e escreve o cabeçalho.
         */
        void initialize(
            const uint64_t original_size,
            const size_t payload_reserve
        ) {
            data.reserve(
                FIXED_HEADER_SIZE + payload_reserve
            );

            data.push_back(MAGIC_0);
            data.push_back(MAGIC_1);
            data.push_back(FORMAT_VERSION);

            /*
             * Será preenchido em finish().
             *
             * 0 = arquivo vazio
             * 1..8 = quantidade de bits válidos no último byte
             */
            data.push_back(0);

            /*
             * Tamanho original em little-endian.
             */
            for (uint8_t shift {0}; shift < 64; shift += 8) {
                data.push_back(
                    static_cast<uint8_t>(
                        (original_size >> shift) & 0xFFU
                    )
                );
            }
        }

        /**
         * Adiciona um código ao fluxo utilizando exatamente
         * o número de bits informado.
         */
        void write(
            const uint16_t value,
            const uint8_t bits
        ) {
            buffer |=
                static_cast<uint64_t>(value) << bit_count;

            bit_count = static_cast<uint8_t>(
                bit_count + bits
            );

            while (bit_count >= 8U) {
                data.push_back(
                    static_cast<uint8_t>(
                        buffer & 0xFFU
                    )
                );

                buffer >>= 8U;

                bit_count = static_cast<uint8_t>(
                    bit_count - 8U
                );
            }
        }

        /**
         * Finaliza o arquivo e grava o último byte parcial.
         */
        std::vector<uint8_t> finish() {
            if (bit_count > 0U) {
                data.push_back(
                    static_cast<uint8_t>(
                        buffer & 0xFFU
                    )
                );

                data[3] = bit_count;
            }
            else {
                /*
                 * Se o arquivo possui payload e terminou
                 * exatamente em um byte, todos os 8 bits
                 * do último byte são válidos.
                 */
                if (data.size() > FIXED_HEADER_SIZE) {
                    data[3] = 8;
                }
                else {
                    /*
                     * Arquivo vazio.
                     */
                    data[3] = 0;
                }
            }

            return std::move(data);
        }
    };

    /**
     * Lê códigos LZW de tamanho variável do fluxo de bits.
     */
    struct BitReader {
        const std::vector<uint8_t>& data;

        size_t byte_index {0};

        uint64_t remaining_bits {0};

        uint64_t buffer {0};
        uint8_t bit_count {0};

        /**
         * Inicializa o leitor sobre o payload.
         */
        BitReader(
            const std::vector<uint8_t>& data,
            const size_t payload_offset,
            const size_t payload_size,
            const uint8_t valid_last_bits
        )
            : data(data),
              byte_index(payload_offset) {
            if (payload_size == 0U) {
                remaining_bits = 0;
                return;
            }

            remaining_bits =
                static_cast<uint64_t>(payload_size - 1U) * 8ULL
                + static_cast<uint64_t>(valid_last_bits);
        }

        /**
         * Lê exatamente `bits` bits do fluxo.
         */
        bool read(
            const uint8_t bits,
            uint16_t& value
        ) {
            if (bits == 0U || bits > 12U) {
                return false;
            }

            if (
                remaining_bits +
                static_cast<uint64_t>(bit_count) <
                static_cast<uint64_t>(bits)
            ) {
                return false;
            }

            /*
             * Carrega bytes inteiros para o buffer enquanto
             * não houver bits suficientes para formar o código.
             *
             * Os bits restantes do byte permanecem no buffer
             * para a próxima leitura.
             */
            while (bit_count < bits) {
                if (remaining_bits == 0U) {
                    return false;
                }

                const uint8_t loaded_bits {
                    remaining_bits >= 8ULL
                        ? static_cast<uint8_t>(8)
                        : static_cast<uint8_t>(remaining_bits)
                };

                const uint64_t mask {
                    (1ULL << loaded_bits) - 1ULL
                };

                buffer |=
                    static_cast<uint64_t>(
                        data[byte_index] & mask
                    ) << bit_count;

                bit_count = static_cast<uint8_t>(
                    bit_count + loaded_bits
                );

                remaining_bits -= loaded_bits;
                ++byte_index;
            }

            /*
             * Os bits menos significativos do buffer representam
             * o próximo código.
             */
            const uint64_t mask {
                (1ULL << bits) - 1ULL
            };

            value = static_cast<uint16_t>(
                buffer & mask
            );

            buffer >>= bits;

            bit_count = static_cast<uint8_t>(
                bit_count - bits
            );

            return true;
        }

        /**
         * Verifica se não existem mais bits válidos no fluxo.
         */
        bool empty() const {
            return
                bit_count == 0U &&
                remaining_bits == 0U;
        }
    };

    /**
     * Determina a largura utilizada pelo compressor para o próximo código.
     */
    static uint8_t get_code_width(
        const uint16_t next_code
    ) {
        if (next_code <= 256U) {
            return 8;
        }

        if (next_code <= 512U) {
            return 9;
        }

        if (next_code <= 1024U) {
            return 10;
        }

        if (next_code <= 2048U) {
            return 11;
        }

        return 12;
    }

    /**
     * Determina a largura do próximo código durante a descompactação.
     *
     * O decoder mantém o dicionário uma entrada atrás do compressor
     * antes de ler o próximo código.
     */
    static uint8_t get_decode_code_width(
        const uint16_t next_code
    ) {
        if (next_code < MAX_DICTIONARY_SIZE) {
            return get_code_width(
                static_cast<uint16_t>(next_code + 1U)
            );
        }

        return 12;
    }

    /**
     * Expande um código existente do dicionário para uma sequência
     * armazenada na ordem inversa dentro de `stack`.
     *
     * Também retorna o primeiro byte da sequência.
     */
    static bool expand_code(
        const uint16_t code,
        const uint16_t next_code,
        const std::array<uint16_t, MAX_DICTIONARY_SIZE>& prefix,
        const std::array<uint8_t, MAX_DICTIONARY_SIZE>& suffix,
        std::array<uint8_t, MAX_DICTIONARY_SIZE>& stack,
        size_t& length,
        uint8_t& first_byte
    ) {
        length = 0;
        uint16_t current_code {code};

        while (current_code >= FIRST_DYNAMIC_CODE) {
            if (
                current_code >= next_code ||
                length >= stack.size()
            ) {
                return false;
            }

            stack[length] = suffix[current_code];
            ++length;

            current_code = prefix[current_code];
        }

        if (length >= stack.size()) {
            return false;
        }

        first_byte = static_cast<uint8_t>(current_code);

        stack[length] = first_byte;
        ++length;

        return true;
    }

public:
    std::vector<uint8_t> apply(
        const std::vector<uint8_t>& data
    ) {
        BitWriter writer;

        writer.initialize(
            static_cast<uint64_t>(data.size()),
            data.size()
        );

        /*
         * Arquivo vazio ainda possui um cabeçalho válido.
         */
        if (data.empty()) {
            return writer.finish();
        }

        /*
         * O compressor pode criar no máximo:
         *
         * data.size() - 1
         *
         * entradas, pois o primeiro byte não gera uma inserção.
         *
         * O dicionário dinâmico possui no máximo:
         *
         * 4096 - 256 = 3840
         *
         * entradas.
         */
        const size_t maximum_dynamic_entries {
            std::min(
                data.size() - 1U,
                static_cast<size_t>(
                    MAX_DICTIONARY_SIZE -
                    FIRST_DYNAMIC_CODE
                )
            )
        };

        /*
         * A tabela é dimensionada uma única vez para esta execução.
         */
        Dictionary dictionary {
            maximum_dynamic_entries
        };

        /*
         * w representa a sequência atual.
         *
         * Os códigos 0..255 são implicitamente os bytes.
         */
        uint16_t w {data[0]};

        for (size_t i {1}; i < data.size(); ++i) {
            const uint8_t k {data[i]};

            uint16_t wk_code {0};

            /*
             * Procura w + k.
             */
            if (dictionary.find(w, k, wk_code)) {
                w = wk_code;
                continue;
            }

            /*
             * w + k não existe:
             *
             * 1. Emite w
             * 2. Adiciona w + k
             * 3. Começa novamente em k
             */
            writer.write(
                w,
                get_code_width(dictionary.next_code)
            );

            dictionary.insert(w, k);

            w = k;
        }

        /*
         * Emite a última sequência.
         */
        writer.write(
            w,
            get_code_width(dictionary.next_code)
        );

        return writer.finish();
    }

    std::vector<uint8_t> deapply(
        const std::vector<uint8_t>& data
    ) {
        /*
         * O arquivo precisa possuir pelo menos o cabeçalho.
         */
        if (data.size() < FIXED_HEADER_SIZE) {
            return {};
        }

        /*
         * Valida o magic.
         */
        if (
            data[0] != MAGIC_0 ||
            data[1] != MAGIC_1
        ) {
            return {};
        }

        /*
         * Valida a versão do formato.
         */
        if (data[2] != FORMAT_VERSION) {
            return {};
        }

        const uint8_t valid_last_bits {data[3]};

        /*
         * Reconstrói o tamanho original armazenado em
         * little-endian.
         */
        uint64_t original_size_64 {0};

        for (uint8_t shift {0}; shift < 64; shift += 8) {
            original_size_64 |=
                static_cast<uint64_t>(
                    data[4U + (shift / 8U)]
                ) << shift;
        }

        /*
         * Não é possível criar um vector maior que size_t.
         */
        if (
            original_size_64 >
            static_cast<uint64_t>(
                std::numeric_limits<size_t>::max()
            )
        ) {
            return {};
        }

        const size_t original_size {
            static_cast<size_t>(original_size_64)
        };

        const size_t payload_size {
            data.size() - FIXED_HEADER_SIZE
        };

        /*
         * Arquivo vazio:
         *
         * não deve possuir payload e não deve declarar
         * bits válidos.
         */
        if (original_size == 0U) {
            if (
                payload_size != 0U ||
                valid_last_bits != 0U
            ) {
                return {};
            }

            return {};
        }

        /*
         * Arquivo não vazio precisa possuir payload.
         */
        if (payload_size == 0U) {
            return {};
        }

        /*
         * Para um payload existente, o último byte precisa possuir
         * entre 1 e 8 bits válidos.
         */
        if (
            valid_last_bits == 0U ||
            valid_last_bits > 8U
        ) {
            return {};
        }

        std::vector<uint8_t> output;

        if (
            original_size >
            output.max_size()
        ) {
            return {};
        }

        output.reserve(original_size);

        /*
         * O decoder não precisa receber o dicionário no arquivo.
         *
         * Cada entrada é representada por:
         *
         * code -> prefix + suffix
         *
         * prefix = código da sequência anterior
         * suffix = byte adicionado ao final
         */
        std::array<uint16_t, MAX_DICTIONARY_SIZE> prefix {};
        std::array<uint8_t, MAX_DICTIONARY_SIZE> suffix {};

        uint16_t next_code {FIRST_DYNAMIC_CODE};

        std::array<uint8_t, MAX_DICTIONARY_SIZE> stack {};

        BitReader reader {
            data,
            FIXED_HEADER_SIZE,
            payload_size,
            valid_last_bits
        };

        /*
         * O primeiro código obrigatoriamente representa
         * um byte literal, portanto possui 8 bits.
         */
        uint16_t previous_code {0};

        if (!reader.read(8U, previous_code)) {
            return {};
        }

        if (previous_code >= FIRST_DYNAMIC_CODE) {
            return {};
        }

        /*
         * Primeiro código = primeiro byte original.
         */
        output.push_back(
            static_cast<uint8_t>(previous_code)
        );

        /*
         * Se já recuperamos tudo, o arquivo deve terminar
         * exatamente neste ponto.
         */
        if (output.size() == original_size) {
            if (!reader.empty()) {
                return {};
            }

            return output;
        }

        while (output.size() < original_size) {
            uint16_t current_code {0};

            /*
             * O decoder está uma entrada atrás do compressor.
             * Por isso a largura do código é calculada com
             * next_code + 1.
             */
            if (
                !reader.read(
                    get_decode_code_width(next_code),
                    current_code
                )
            ) {
                return {};
            }

            /*
             * Códigos maiores que next_code são inválidos.
             *
             * current_code == next_code é o caso especial
             * conhecido como KwKwK.
             */
            if (
                current_code > next_code ||
                (
                    current_code == next_code &&
                    next_code >= MAX_DICTIONARY_SIZE
                )
            ) {
                return {};
            }

            size_t sequence_length {0};
            uint8_t first_byte {0};

            if (current_code == next_code) {
                /*
                 * Caso especial:
                 *
                 * sequência atual =
                 * sequência anterior + primeiro byte da
                 * sequência anterior.
                 *
                 * expand_code() armazena a sequência invertida.
                 * Portanto, o primeiro byte deve ser inserido
                 * no início do stack.
                 */
                if (
                    !expand_code(
                        previous_code,
                        next_code,
                        prefix,
                        suffix,
                        stack,
                        sequence_length,
                        first_byte
                    )
                ) {
                    return {};
                }

                if (sequence_length >= stack.size()) {
                    return {};
                }

                for (
                    size_t i {sequence_length};
                    i > 0U;
                    --i
                ) {
                    stack[i] = stack[i - 1U];
                }

                stack[0] = first_byte;

                ++sequence_length;
            }
            else {
                if (
                    !expand_code(
                        current_code,
                        next_code,
                        prefix,
                        suffix,
                        stack,
                        sequence_length,
                        first_byte
                    )
                ) {
                    return {};
                }
            }

            /*
             * Impede que uma entrada inválida faça a saída
             * ultrapassar o tamanho original informado no cabeçalho.
             */
            if (
                sequence_length >
                original_size - output.size()
            ) {
                return {};
            }

            /*
             * expand_code() produziu a sequência ao contrário.
             * Escrevemos do final para o início para recuperar
             * a ordem original.
             */
            for (
                size_t i {sequence_length};
                i > 0U;
                --i
            ) {
                output.push_back(
                    stack[i - 1U]
                );
            }

            /*
             * O decoder agora pode reconstruir a mesma entrada
             * que o compressor adicionou.
             *
             * new_code =
             * previous_sequence + first_byte(current_sequence)
             */
            if (next_code < MAX_DICTIONARY_SIZE) {
                prefix[next_code] = previous_code;
                suffix[next_code] = first_byte;

                ++next_code;
            }

            previous_code = current_code;
        }

        /*
         * Depois de recuperar exatamente o tamanho original,
         * todos os bits válidos do payload precisam ter sido
         * consumidos. O que poderia restar seria somente padding,
         * e esse padding já foi excluído pelo BitReader.
         */
        if (!reader.empty()) {
            return {};
        }

        return output;
    }
};