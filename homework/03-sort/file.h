#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED

#include "config.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

typedef unsigned long long ull;

template <typename T>
class File {
protected:
    FILE* file = nullptr;
    std::string fopen_desc;
    bool eof_flag;

public:
    File() {}

    File(FILE* file, bool check_opened = true, std::string fopen_desc = std::string()) {
        opened(file, check_opened);
    }

    File(const char* fname, const char* mode, bool check_opened = true) {
        opened(fname, mode, check_opened);
    }

    File(const File&) = delete;
    File(File&&) = default;

    ~File() {
        close();
    }

    bool inUse() {
        return this->file != nullptr;
    }

    void opened(FILE* file, bool check_opened = true, std::string fopen_desc = std::string()) {
        if (this->file != nullptr) {
            throw std::runtime_error("This instance of File is already in use");
        } else if (check_opened && file == nullptr) {
            auto errnum = errno;
            auto errmsg = strerror(errnum);

            std::ostringstream os;
            os << "Failed to open file \'" << fopen_desc
               << " (errno=" << errnum << ": " << errmsg << ")";
            throw std::runtime_error(os.str());
        }

        this->file = file;
        this->fopen_desc = fopen_desc;
        this->eof_flag = false;
    }

    void opened(const char* fname, const char* mode, bool check_opened = true) {
        std::string fopen_desc = std::string();
        if (check_opened) {
            std::ostringstream os;
            os << "\'" << fname << "\' with mode \'" << mode << "\'";
            fopen_desc = os.str();
        }

        opened(fopen(fname, mode), check_opened, fopen_desc);
    }

    void write(const T* buf, size_t buf_len) {
        assert_usage();
        size_t wrtcnt = fwrite(buf, sizeof(T), buf_len, file);
        if (wrtcnt != buf_len) {
            auto errnum = errno;
            auto errmsg = strerror(errnum);

            std::ostringstream os;
            os << "Failed to write " << wrtcnt << "objects to file \'" << fopen_desc << "\' "
               << "(errno=" << errnum << ": " << errmsg << ")";
            throw std::runtime_error(os.str());
        }
    }

    size_t read(T* buf, size_t buf_len) {
        assert_usage();
        size_t rdcnt = fread(buf, sizeof(T), buf_len, file);
        if (ferror(file)) {
            auto errnum = errno;
            auto errmsg = strerror(errnum);

            std::ostringstream os;
            os << "Failed after reading " << rdcnt << "objects from file \'" << fopen_desc << "\' "
               << "(errno=" << errnum << ": " << errmsg << ")";
            throw std::runtime_error(os.str());
        }
        if (rdcnt != buf_len && !feof(file)) {
            int c = fgetc(file);
            if (c != EOF) {
                ungetc(c, file);
                throw std::runtime_error("File size is not aligned to sizeof(object)");
            }
            eof_flag = true;
        }

        return rdcnt;
    }

    void flush() {
        fflush(file);
    }

    void rewind() {
        flush();
        ::rewind(file);
    }

    void close() {
        if (file != nullptr) {
            fclose(file);
        }
        file = nullptr;
    }

    bool eof() {
        assert_usage();
        return eof_flag;
    }

protected:
    void assert_usage() {
        if (file == nullptr) {
            throw std::runtime_error("Init (call File.opened(FILE*)) before using this File instance");
        }
    }
};

template <typename T>
std::vector<File<T> > TempFiles(size_t count) {
    std::vector<File<T> > f(count);
    for (auto it = f.begin(); it != f.end(); ++it) {
        it->opened(tmpfile(), true, "<tempfile>");
    }

    return f;
}

template <typename T>
class FileBuf {
protected:
    File<T>& f;
    T* buf;
    size_t buf_size;

    FileBuf(File<T>& f, T* buf_first, T* buf_last)
        : f(f)
        , buf(buf_first)
        , buf_size(buf_last - buf_first) {
        assert(buf_first < buf_last);
    }
};

template <typename T>
class FileReader : public FileBuf<T> {
protected:
    size_t buf_cur;
    size_t buf_top;

public:
    FileReader(File<T>& f, T* buf_first, T* buf_last)
        : FileBuf<T>(f, buf_first, buf_last)
        , buf_cur(this->buf_size)
        , buf_top(this->buf_size) {
        assert(f.eof());
    }

    ~FileReader() {
        if (buf_cur != buf_top) {
            std::cout << "WRN: " << buf_top - buf_cur << " elements left in FileReader buf" << std::endl;
        }
    }

    bool eof() {
        assert(buf_cur <= buf_top);

        return this->f.eof() && buf_cur >= buf_top;
    }

    virtual bool get(T* out) {
        assert(buf_cur <= buf_top);

        if (buf_cur >= buf_top) {
            buf_top = this->f.read(this->buf, this->buf_size);
            buf_cur = 0;
            if (!buf_top) {
                return false;
            }
        }

        *out = this->buf[buf_cur++];
        return true;
    }
};

template <typename T>
class BarrieredFileReader : public FileReader<T> {
protected:
    ull rdcnt;
    ull barrier_ts;

public:
    BarrieredFileReader(File<T>& f, ull barrier_ts, T* buf_first, T* buf_last)
        : FileReader<T>(f, buf_first, buf_last)
        , rdcnt(0)
        , barrier_ts(barrier_ts) {
        assert(barrier_ts > 0);
    }

    bool get(T* out) override // WRN: heavily using virtual function
    {
        assert(rdcnt <= barrier_ts);

        bool success = rdcnt < barrier_ts && this->FileReader<T>::get(out);
        rdcnt += success;
        return success;
    }

    bool barrier() {
        assert(rdcnt <= barrier_ts);

        return rdcnt >= barrier_ts;
    }

    void proceed() {
        assert(barrier());
        assert(!this->eof());

        rdcnt = 0;
    }
};

template <typename T>
class FileWriter : public FileBuf<T> {
protected:
    size_t buf_top;

public:
    FileWriter(File<T>& f, T* buf_first, T* buf_last)
        : FileBuf<T>(f, buf_first, buf_last)
        , buf_top(0) {}

    ~FileWriter() {
        flush();
    }

    virtual bool put(T value) {
        assert(buf_top < this->buf_size);

        this->buf[buf_top++] = value;

        if (buf_top >= this->buf_size) {
            flush(false);
        }
    }

    void flush(bool deep = true) {
        this->f.write(this->buf, buf_top);
        buf_top = 0;

        if (deep) {
            this->f.flush();
        }
    }
};

#endif // FILE_H_INCLUDED
