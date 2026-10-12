// customers.cpp - SQLite + C++ CRUD example
// Build: g++ -std=c++17 -Wall -o customers customers.cpp -lsqlite3

#include <sqlite3.h>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct Customer {
    int id = 0;
    std::string name;
    std::string address;
    std::string phone;
};

// RAII wrapper for a prepared statement so it is always finalized.
class Stmt {
public:
    Stmt(sqlite3* db, const std::string& sql) : db_(db) {
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
            throw std::runtime_error(std::string("prepare failed: ") + sqlite3_errmsg(db_));
    }
    ~Stmt() { sqlite3_finalize(stmt_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    void bindText(int idx, const std::string& v) {
        // SQLITE_TRANSIENT makes SQLite copy the string.
        sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    void bindInt(int idx, int v) { sqlite3_bind_int(stmt_, idx, v); }

    // Returns true if a row is available (SQLITE_ROW), false when done.
    bool step() {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("step failed: ") + sqlite3_errmsg(db_));
    }

    int colInt(int i) const { return sqlite3_column_int(stmt_, i); }
    std::string colText(int i) const {
        const unsigned char* t = sqlite3_column_text(stmt_, i);
        return t ? reinterpret_cast<const char*>(t) : "";
    }

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

class CustomerDb {
public:
    explicit CustomerDb(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            throw std::runtime_error("open failed: " + msg);
        }
        createTable();
    }
    ~CustomerDb() { sqlite3_close(db_); }
    CustomerDb(const CustomerDb&) = delete;
    CustomerDb& operator=(const CustomerDb&) = delete;

    // CREATE
    int add(const Customer& c) {
        Stmt s(db_, "INSERT INTO customers (name, address, phone) VALUES (?, ?, ?);");
        s.bindText(1, c.name);
        s.bindText(2, c.address);
        s.bindText(3, c.phone);
        s.step();
        return static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    // READ (one)
    std::optional<Customer> get(int id) {
        Stmt s(db_, "SELECT id, name, address, phone FROM customers WHERE id = ?;");
        s.bindInt(1, id);
        if (!s.step()) return std::nullopt;
        return Customer{s.colInt(0), s.colText(1), s.colText(2), s.colText(3)};
    }

    // READ (all)
    std::vector<Customer> list() {
        std::vector<Customer> out;
        Stmt s(db_, "SELECT id, name, address, phone FROM customers ORDER BY id;");
        while (s.step())
            out.push_back({s.colInt(0), s.colText(1), s.colText(2), s.colText(3)});
        return out;
    }

    // UPDATE - returns true if a row was changed
    bool update(const Customer& c) {
        Stmt s(db_, "UPDATE customers SET name = ?, address = ?, phone = ? WHERE id = ?;");
        s.bindText(1, c.name);
        s.bindText(2, c.address);
        s.bindText(3, c.phone);
        s.bindInt(4, c.id);
        s.step();
        return sqlite3_changes(db_) > 0;
    }

    // DELETE - returns true if a row was removed
    bool remove(int id) {
        Stmt s(db_, "DELETE FROM customers WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_) > 0;
    }

private:
    void createTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS customers ("
            "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name    TEXT NOT NULL,"
            "  address TEXT,"
            "  phone   TEXT"
            ");";
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown";
            sqlite3_free(err);
            throw std::runtime_error("create table failed: " + msg);
        }
    }

    sqlite3* db_ = nullptr;
};

static void print(const Customer& c) {
    std::cout << "  [" << c.id << "] " << c.name << " | " << c.address << " | " << c.phone << "\n";
}

static void printAll(CustomerDb& db, const std::string& title) {
    std::cout << title << "\n";
    auto rows = db.list();
    if (rows.empty()) std::cout << "  (empty)\n";
    for (const auto& c : rows) print(c);
}

int main() {
    try {
        CustomerDb db("customers.db");

        // Create
        int a = db.add({0, "Ada Lovelace", "12 Analytical Way", "555-0101"});
        int b = db.add({0, "Alan Turing", "1 Bombe Lane", "555-0102"});
        db.add({0, "Grace Hopper", "9 Compiler Court", "555-0103"});
        printAll(db, "After inserts:");

        // Read one
        if (auto c = db.get(a)) { std::cout << "Fetched id " << a << ":\n"; print(*c); }

        // Update
        Customer upd{b, "Alan M. Turing", "2 Enigma Street", "555-9999"};
        std::cout << "Update id " << b << ": " << (db.update(upd) ? "ok" : "not found") << "\n";
        printAll(db, "After update:");

        // Delete
        std::cout << "Delete id " << a << ": " << (db.remove(a) ? "ok" : "not found") << "\n";
        std::cout << "Delete id 999: " << (db.remove(999) ? "ok" : "not found") << "\n";
        printAll(db, "After delete:");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}