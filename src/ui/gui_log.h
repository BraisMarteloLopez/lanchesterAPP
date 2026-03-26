// gui_log.h — Sistema de logging para la GUI
#pragma once

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

class GuiLog {
public:
    static GuiLog& instance() {
        static GuiLog log;
        return log;
    }

    // Inicializar con ruta del fichero (junto al .exe)
    void init(const std::string& exe_dir) {
        std::lock_guard<std::mutex> lock(mutex_);
        path_ = exe_dir + "/lanchester.log";
        file_.open(path_, std::ios::out | std::ios::trunc);
        if (file_.is_open()) {
            write_impl("INFO", "Log iniciado: %s", path_.c_str());
        }
    }

    void info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        write_va("INFO", fmt, args);
        va_end(args);
    }

    void warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        write_va("WARN", fmt, args);
        va_end(args);
    }

    void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        write_va("ERROR", fmt, args);
        va_end(args);
    }

    const std::string& path() const { return path_; }

private:
    GuiLog() = default;

    void write_va(const char* level, const char* fmt, va_list args) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        std::lock_guard<std::mutex> lock(mutex_);
        write_impl(level, "%s", buf);
    }

    void write_impl(const char* level, const char* fmt, ...) {
        if (!file_.is_open()) return;

        // Timestamp
        std::time_t now = std::time(nullptr);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&now));

        // Mensaje
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        file_ << "[" << ts << "] [" << level << "] " << buf << "\n";
        file_.flush();
    }

    std::string path_;
    std::ofstream file_;
    std::mutex mutex_;
};

// Macros de conveniencia
#define LOG_INFO(...)  GuiLog::instance().info(__VA_ARGS__)
#define LOG_WARN(...)  GuiLog::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) GuiLog::instance().error(__VA_ARGS__)
