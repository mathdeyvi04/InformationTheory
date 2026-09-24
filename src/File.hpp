#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

class File {
private:
    enum class Mode {
        READ,
        WRITE
    };
    Mode m_mode;

    std::ifstream m_fileRead;
    std::ofstream m_fileWrite;
public:

    File(const std::string& filename, bool is_reader) {

        if(is_reader) {
            m_fileRead.open(filename, std::ios::binary);

            if(m_fileRead.is_open()) {
                m_mode = Mode::READ;
                return;
            }
        }

        m_fileWrite.open(
            filename,
            std::ios::binary | std::ios::out
        );

        m_mode = Mode::WRITE;
        return;
    }

    ~File() {
        if (m_fileRead.is_open()) {
            m_fileRead.close();
        }
        if (m_fileWrite.is_open()) {
            m_fileWrite.close();
        }
    }

    std::vector<uint8_t> read() {

        // Arquivo foi aberto para escrita
        if(m_mode != Mode::READ || !m_fileRead.is_open()) {
            return {};
        }

        m_fileRead.seekg(0, std::ios::end);
        std::streamsize size = m_fileRead.tellg();

        m_fileRead.seekg(0, std::ios::beg);

        if(size < 0) {
            return {};
        }

        std::vector<uint8_t> data(
            static_cast<size_t>(size)
        );

        if(size > 0) {
            m_fileRead.read(
                reinterpret_cast<char*>(data.data()),
                size
            );
        }

        return data;
    }

    void write(const std::vector<uint8_t>& data) {

        // Arquivo foi aberto para leitura
        if(m_mode != Mode::WRITE || !m_fileWrite.is_open()) {
            return;
        }

        if(data.empty()) {
            return;
        }

        m_fileWrite.write(
            reinterpret_cast<const char*>(data.data()),
            data.size()
        );
    }
};