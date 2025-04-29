// src/main.cpp  ── C++17-only edition ────────────────────────────
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

const int         PORT     = 8080;
const std::string CSV_FILE = "data/customers.csv";
const std::string HEADER   = "id,name,surname,email,phone";

/* ─────────────────────────── C++17 helpers ─────────────────────────── */
inline bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), s.begin());
}
inline bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

/* ─────────────────────────── Data model ────────────────────────────── */
struct Customer {
    int id;
    std::string name, surname, email, phone;
};

std::vector<Customer> readCSV() {
    std::vector<Customer> v;
    std::ifstream f(CSV_FILE);
    if (!f.is_open()) return v;
    std::string line;
    getline(f, line);                     // skip header
    while (getline(f, line) && !line.empty()) {
        std::stringstream ss(line);
        std::string cell;
        Customer c;
        getline(ss, cell, ','); c.id      = std::stoi(cell);
        getline(ss, c.name,    ',');
        getline(ss, c.surname, ',');
        getline(ss, c.email,   ',');
        getline(ss, c.phone,   ',');
        v.emplace_back(c);
    }
    return v;
}

void writeCSV(const std::vector<Customer>& v) {
    std::ofstream f(CSV_FILE, std::ios::trunc);
    f << HEADER << '\n';
    for (auto& c : v)
        f << c.id << ',' << c.name << ',' << c.surname << ','
          << c.email << ',' << c.phone << '\n';
}

/* ─────────────────────────── HTTP helpers ──────────────────────────── */
struct Request { std::string method, path, body; };

Request parseReq(const std::string& raw) {
    std::istringstream ss(raw);
    Request r;
    ss >> r.method >> r.path;                   // ignore HTTP version
    r.body = raw.substr(raw.find("\r\n\r\n") + 4);
    return r;
}

void reply(int sock, const std::string& status,
           const std::string& ctype, const std::string& body) {
    std::ostringstream o;
    o << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << ctype << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n"
      << "Access-Control-Allow-Headers: Content-Type\r\n"
      << "Content-Length: " << body.size() << "\r\n\r\n"
      << body;
    send(sock, o.str().c_str(), o.str().size(), 0);
}

/* ─────────────────────────── URL-encoded helpers ───────────────────── */
Customer url2cust(const std::string& s, int forcedId = -1) {
    auto val = [&](const std::string& key) -> std::string {
        auto pos = s.find(key + '=');
        if (pos == std::string::npos) return {};
        auto end = s.find('&', pos);
        if (end == std::string::npos) end = s.size();
        return s.substr(pos + key.size() + 1, end - pos - key.size() - 1);
    };
    Customer c;
    c.id      = forcedId;
    c.name    = val("name");
    c.surname = val("surname");
    c.email   = val("email");
    c.phone   = val("phone");
    return c;
}

/* ─────────────────────────── API routing ───────────────────────────── */
void handleApi(int sock, const Request& q) {
    auto data = readCSV();

    /* CORS pre-flight */
    if (q.method == "OPTIONS") {
        reply(sock, "204 No Content", "text/plain", "");
        return;
    }

    const std::string base = "/api/customers";
    if (!starts_with(q.path, base)) {
        reply(sock, "404 Not Found", "text/plain", "Bad endpoint");
        return;
    }
    std::string idStr;
    if (q.path.size() > base.size() + 1)
        idStr = q.path.substr(base.size() + 1);     // skip '/'

    /* ---------- GET ---------- */
    if (q.method == "GET") {
        std::ostringstream csv;
        csv << HEADER << '\n';
        if (idStr.empty()) {                // list
            for (auto& c : data)
                csv << c.id << ',' << c.name << ',' << c.surname << ','
                    << c.email << ',' << c.phone << '\n';
        } else {                            // single
            int id = std::stoi(idStr);
            auto it = std::find_if(data.begin(), data.end(),
                                   [&](auto& c) { return c.id == id; });
            if (it == data.end())
                return reply(sock, "404 Not Found", "text/plain", "no such customer");
            auto& c = *it;
            csv << c.id << ',' << c.name << ',' << c.surname << ','
                << c.email << ',' << c.phone << '\n';
        }
        return reply(sock, "200 OK", "text/csv", csv.str());
    }

    /* ---------- POST (create) ---------- */
    if (q.method == "POST" && idStr.empty()) {
        int newId = data.empty() ? 1 : data.back().id + 1;
        Customer c = url2cust(q.body, newId);
        data.push_back(c);
        writeCSV(data);
        std::ostringstream csv;
        csv << HEADER << '\n'
            << c.id << ',' << c.name << ',' << c.surname << ','
            << c.email << ',' << c.phone << '\n';
        return reply(sock, "201 Created", "text/csv", csv.str());
    }

    /* ---------- PUT (update) ---------- */
    if (q.method == "PUT" && !idStr.empty()) {
        int id = std::stoi(idStr);
        auto it = std::find_if(data.begin(), data.end(),
                               [&](auto& c) { return c.id == id; });
        if (it == data.end())
            return reply(sock, "404 Not Found", "text/plain", "no such customer");
        *it = url2cust(q.body, id);             // overwrite
        writeCSV(data);
        return reply(sock, "200 OK", "text/plain", "updated");
    }

    /* ---------- DELETE ---------- */
    if (q.method == "DELETE" && !idStr.empty()) {
        int id = std::stoi(idStr);
        auto old = data.size();
        data.erase(std::remove_if(data.begin(), data.end(),
                                  [&](auto& c) { return c.id == id; }),
                    data.end());
        if (data.size() == old)
            return reply(sock, "404 Not Found", "text/plain", "no such customer");
        writeCSV(data);
        return reply(sock, "204 No Content", "text/plain", "");
    }

    reply(sock, "405 Method Not Allowed", "text/plain", "bad verb");
}

/* ─────────────────────────── Static files ──────────────────────────── */
void handleStatic(int sock, const std::string& path) {
    std::string file = "static" + (path == "/" ? "/index.html" : path);
    std::ifstream f(file, std::ios::binary);
    if (!f.is_open())
        return reply(sock, "404 Not Found", "text/plain", "not found");

    std::ostringstream s;
    s << f.rdbuf();
    std::string ctype = ends_with(file, ".css") ? "text/css" :
                        ends_with(file, ".js")  ? "text/javascript" :
                                                  "text/html";
    reply(sock, "200 OK", ctype, s.str());
}

/* ─────────────────────────── Connection loop ───────────────────────── */
void handleClient(int sock) {
    char buf[8192] = {0};
    int len = read(sock, buf, sizeof(buf));
    if (len <= 0) { close(sock); return; }

    Request r = parseReq(buf);
    if (starts_with(r.path, "/api/"))
        handleApi(sock, r);
    else
        handleStatic(sock, r.path);

    close(sock);
}

int main() {
    int serv = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(serv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(serv, 10);

    std::cout << "Listening on http://localhost:" << PORT << '\n';
    while (true) {
        sockaddr_in cliAddr{};
        socklen_t   sz = sizeof(cliAddr);
        int cli = accept(serv, reinterpret_cast<sockaddr*>(&cliAddr), &sz);
        if (cli >= 0) handleClient(cli);
    }
}

