
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cctype>

using namespace std;

// ================= ACCOUNT CLASS =================

class Account {
private:
    int id;
    string name;
    string phone;
    double balance;

public:
    Account(int i, string n, string p, double b) {
        id = i;
        name = n;
        phone = p;
        balance = b;
    }

    int getId() const { return id; }
    string getName() const { return name; }
    string getPhone() const { return phone; }
    double getBalance() const { return balance; }

    bool deposit(double amount) {
        if (amount <= 0) return false;
        balance += amount;
        return true;
    }

    bool withdraw(double amount) {
        if (amount <= 0 || amount > balance)
            return false;

        balance -= amount;
        return true;
    }
};

// ================= TRANSACTION CLASS =================

class Transaction {
private:
    int id;
    string type;
    int fromId;
    int toId;
    double amount;
    string time;

public:
    Transaction(int i, string t, int from, int to,
                double a, string tm) {
        id = i;
        type = t;
        fromId = from;
        toId = to;
        amount = a;
        time = tm;
    }

    int getId() const { return id; }
    string getType() const { return type; }
    int getFromId() const { return fromId; }
    int getToId() const { return toId; }
    double getAmount() const { return amount; }
    string getTime() const { return time; }
};

// ================= DATA STORAGE =================

vector<Account> accounts;
vector<Transaction> transactions;

int nextAccountId = 1001;
int nextTransactionId = 1;

// ================= HELPER FUNCTIONS =================

string lowerText(string s) {
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)tolower(c); });
    return s;
}

string jsonEscape(const string& s) {
    string out;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c >= 32) out += c;
    }

    return out;
}

string moneyText(double amount) {
    ostringstream out;
    out << fixed << setprecision(2) << amount;
    return out.str();
}

string currentTime() {
    time_t now = time(NULL);
    tm* t = localtime(&now);

    char buffer[32];

    if (t)
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
    else
        strcpy(buffer, "Unknown");

    return buffer;
}

string urlDecode(const string& s) {
    string out;

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        }
        else if (s[i] == '%' && i + 2 < s.size()) {
            string hex = s.substr(i + 1, 2);
            char* end = NULL;
            long value = strtol(hex.c_str(), &end, 16);

            if (end && *end == '\0') {
                out += (char)value;
                i += 2;
            }
            else {
                out += s[i];
            }
        }
        else {
            out += s[i];
        }
    }

    return out;
}

string fieldValue(const string& body, const string& key) {
    stringstream ss(body);
    string part;

    while (getline(ss, part, '&')) {
        size_t eq = part.find('=');

        if (eq == string::npos) continue;

        string k = urlDecode(part.substr(0, eq));

        if (k == key)
            return urlDecode(part.substr(eq + 1));
    }

    return "";
}

bool parsePositiveAmount(const string& text, double& amount) {
    if (text.empty()) return false;

    char* end = NULL;
    amount = strtod(text.c_str(), &end);

    if (end == text.c_str() || *end != '\0')
        return false;

    if (amount <= 0 || amount > 1000000000000.0)
        return false;

    double cents = amount * 100.0;

    if (cents - (long long)(cents + 0.0000001) > 0.000001)
        return false;

    amount = (long long)(cents + 0.0000001) / 100.0;
    return true;
}

// ================= FIND ACCOUNT =================

Account* findAccount(int id) {
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (accounts[i].getId() == id)
            return &accounts[i];
    }

    return NULL;
}

// ================= JSON FUNCTIONS =================

string accountsJson() {
    ostringstream out;
    out << "[";

    for (size_t i = 0; i < accounts.size(); ++i) {
        if (i) out << ",";

        out << "{\"id\":" << accounts[i].getId()
            << ",\"name\":\"" << jsonEscape(accounts[i].getName())
            << "\",\"phone\":\"" << jsonEscape(accounts[i].getPhone())
            << "\",\"balance\":" << moneyText(accounts[i].getBalance())
            << "}";
    }

    out << "]";
    return out.str();
}

string transactionsJson() {
    ostringstream out;
    out << "[";

    for (size_t i = 0; i < transactions.size(); ++i) {
        const Transaction& t = transactions[i];

        if (i) out << ",";

        out << "{\"id\":" << t.getId()
            << ",\"type\":\"" << jsonEscape(t.getType())
            << "\",\"fromId\":" << t.getFromId()
            << ",\"toId\":" << t.getToId()
            << ",\"amount\":" << moneyText(t.getAmount())
            << ",\"time\":\"" << jsonEscape(t.getTime())
            << "\"}";
    }

    out << "]";
    return out.str();
}

string responseJson(bool ok, const string& message) {
    return string("{\"ok\":") + (ok ? "true" : "false") +
        ",\"message\":\"" + jsonEscape(message) + "\"}";
}

// ================= ADD TRANSACTION =================

void addTransaction(const string& type, int fromId,
                    int toId, double amount) {
    Transaction t(
        nextTransactionId++,
        type,
        fromId,
        toId,
        amount,
        currentTime()
    );

    transactions.push_back(t);
}

// ================= BANKING API =================

string handleApi(const string& method, const string& path,
                 const string& body) {

    if (method == "GET" && path == "/api/accounts")
        return string("{\"ok\":true,\"accounts\":") +
            accountsJson() + "}";

    if (method == "GET" && path == "/api/transactions")
        return string("{\"ok\":true,\"transactions\":") +
            transactionsJson() + "}";

    if (method != "POST")
        return responseJson(false, "Unsupported request method.");

    // CREATE ACCOUNT
    if (path == "/api/add") {
        string name = fieldValue(body, "name");
        string phone = fieldValue(body, "phone");
        string openingText = fieldValue(body, "initial");
        double opening = 0.0;

        if (name.empty() || phone.empty())
            return responseJson(false,
                "Enter the customer name and phone number.");

        if (name.size() > 80 || phone.size() > 20)
            return responseJson(false,
                "Name or phone number is too long.");

        if (!openingText.empty() &&
            !parsePositiveAmount(openingText, opening)) {

            if (openingText != "0" && openingText != "0.00")
                return responseJson(false,
                    "Opening balance must be zero or a valid positive amount.");
        }

        Account a(nextAccountId++, name, phone, opening);
        int newId = a.getId();

        accounts.push_back(a);

        if (opening > 0)
            addTransaction("Opening deposit", 0, newId, opening);

        return responseJson(true,
            "Account created successfully. Account number: " +
            to_string(newId));
    }

    // DEPOSIT OR WITHDRAW
    if (path == "/api/deposit" || path == "/api/withdraw") {
        int id = atoi(fieldValue(body, "accountId").c_str());
        double amount = 0.0;

        Account* a = findAccount(id);

        if (!a)
            return responseJson(false, "Account not found.");

        if (!parsePositiveAmount(fieldValue(body, "amount"), amount))
            return responseJson(false,
                "Enter an amount greater than zero with at most two decimals.");

        if (path == "/api/deposit") {
            a->deposit(amount);

            addTransaction("Deposit", 0, id, amount);

            return responseJson(true,
                "Deposit completed successfully.");
        }

        if (!a->withdraw(amount))
            return responseJson(false, "Insufficient balance.");

        addTransaction("Withdrawal", id, 0, amount);

        return responseJson(true,
            "Withdrawal completed successfully.");
    }

    // TRANSFER MONEY
    if (path == "/api/transfer") {
        int fromId = atoi(fieldValue(body, "fromId").c_str());
        int toId = atoi(fieldValue(body, "toId").c_str());
        double amount = 0.0;

        Account* from = findAccount(fromId);
        Account* to = findAccount(toId);

        if (!from || !to)
            return responseJson(false,
                "Both accounts must exist.");

        if (fromId == toId)
            return responseJson(false,
                "Choose two different accounts.");

        if (!parsePositiveAmount(fieldValue(body, "amount"), amount))
            return responseJson(false,
                "Enter an amount greater than zero with at most two decimals.");

        if (from->getBalance() < amount)
            return responseJson(false, "Insufficient balance.");

        if (!from->withdraw(amount))
            return responseJson(false, "Transfer failed.");

        to->deposit(amount);

        addTransaction("Transfer", fromId, toId, amount);

        return responseJson(true,
            "Transfer completed successfully.");
    }

    return responseJson(false, "API endpoint not found.");
}

// ================= FILE READING =================

string readFile(const string& filename) {
    ifstream file(filename.c_str(), ios::binary);

    if (!file) return "";

    stringstream contents;
    contents << file.rdbuf();

    return contents.str();
}

string getMimeType(const string& path) {
    if (path == "/style.css")
        return "text/css; charset=utf-8";

    if (path == "/script.js")
        return "application/javascript; charset=utf-8";

    return "text/html; charset=utf-8";
}

string lowerHeader(const string& s) {
    return lowerText(s);
}

// ================= HTTP REQUEST =================

bool readRequest(SOCKET client, string& request) {
    char buffer[4096];
    request.clear();

    size_t headerEnd = string::npos;
    int contentLength = 0;

    while (true) {
        int received = recv(client, buffer, sizeof(buffer), 0);

        if (received <= 0) return false;

        request.append(buffer, received);

        if (headerEnd == string::npos) {
            headerEnd = request.find("\r\n\r\n");

            if (headerEnd != string::npos) {
                string headers = request.substr(0, headerEnd);
                stringstream lines(headers);
                string line;

                getline(lines, line);

                while (getline(lines, line)) {
                    if (!line.empty() &&
                        line[line.size() - 1] == '\r')
                        line.erase(line.size() - 1);

                    string lower = lowerHeader(line);

                    if (lower.find("content-length:") == 0) {
                        contentLength = atoi(
                            line.substr(line.find(':') + 1).c_str());
                    }
                }
            }
        }

        if (headerEnd != string::npos) {
            size_t bodyStart = headerEnd + 4;

            if (request.size() >=
                bodyStart + (size_t)contentLength)
                return true;
        }

        if (request.size() > 1024 * 1024)
            return false;
    }
}

// ================= HTTP RESPONSE =================

void sendResponse(SOCKET client, const string& status,
                  const string& contentType, const string& body) {
    ostringstream response;

    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n"
             << "X-Content-Type-Options: nosniff\r\n\r\n"
             << body;

    string data = response.str();
    size_t sentTotal = 0;

    while (sentTotal < data.size()) {
        int sent = send(client,
            data.c_str() + sentTotal,
            (int)(data.size() - sentTotal), 0);

        if (sent <= 0) break;

        sentTotal += sent;
    }
}

// ================= PROCESS CLIENT =================

void processClient(SOCKET client) {
    string request;

    if (!readRequest(client, request)) {
        closesocket(client);
        return;
    }

    size_t headerEnd = request.find("\r\n\r\n");

    string headers = request.substr(0, headerEnd);
    string body = request.substr(headerEnd + 4);

    stringstream firstLine(headers);
    string method, path, version;

    firstLine >> method >> path >> version;

    // FRONTEND FILES
    if (method == "GET" &&
        (path == "/" || path == "/index.html" ||
         path == "/style.css" || path == "/script.js")) {

        string filename = (path == "/" || path == "/index.html")
            ? "index.html" : path.substr(1);

        string page = readFile(filename);

        if (page.empty()) {
            sendResponse(client, "404 Not Found",
                "text/plain",
                "Frontend file not found. Run BankSphere.exe from the project folder.");
        }
        else {
            sendResponse(client, "200 OK",
                getMimeType(path), page);
        }
    }
    else if (path.find("/api/") == 0) {
        string result = handleApi(method, path, body);

        sendResponse(client, "200 OK",
            "application/json; charset=utf-8", result);
    }
    else {
        sendResponse(client, "404 Not Found",
            "text/plain", "Not found.");
    }

    closesocket(client);
}

// ================= MAIN FUNCTION =================

int main() {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Winsock startup failed.\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server == INVALID_SOCKET) {
        cout << "Could not create socket.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server, (sockaddr*)&address,
             sizeof(address)) == SOCKET_ERROR) {
        cout << "Could not use port 8080. Close another server using it.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, 10) == SOCKET_ERROR) {
        cout << "Could not start listening.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    cout << "\n=====================================\n";
    cout << "       BankSphere Banking System\n";
    cout << "=====================================\n";
    cout << "Storage: temporary memory only\n";
    cout << "Open: http://127.0.0.1:8080\n";
    cout << "Keep this window open while using it.\n";
    cout << "Press Ctrl+C to stop the server.\n\n";

    while (true) {
        SOCKET client = accept(server, NULL, NULL);

        if (client == INVALID_SOCKET)
            continue;

        processClient(client);
    }

    closesocket(server);
    WSACleanup();

    return 0;
}
