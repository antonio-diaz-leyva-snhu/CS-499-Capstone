// =====================================================================
// CS-499 Capstone - Database Artifact
// Author: Antonio Diaz-Leyva
// =====================================================================

#include <iostream>
#include <string>
#include <limits>
#include <functional>
#include <cctype>
#include <sqlite3.h>

// Cross-platform helper so the password is not echoed to the screen
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

// Main menu options. Exit stays last; the filtered report will be added before it.
enum MenuOption
{
    MENU_DISPLAY = 1,
    MENU_ADD = 2,
    MENU_CHANGE = 3,
    MENU_DELETE = 4,
    MENU_REPORT = 5,
    MENU_EXIT = 6
};

string username;

const int MAX_LOGIN_ATTEMPTS = 3; // current hardcoded login attempts
const size_t MAX_USERNAME_LENGTH = 32;
const size_t MIN_PASSWORD_LENGTH = 6;

// Database Setup
const string DATABASE_FILE = "clients.db"; // file path
const bool SHOW_DB_ERROR_DETAILS = false;  // set to true when developing sql prints

// ---- Function prototypes ----
int CheckUserPermissionAccess(sqlite3 *db);
void DisplayInfo(sqlite3 *db);
void ChangeCustomerChoice(sqlite3 *db);
string GetMaskedPassword();
size_t HashPassword(const string &plainText);
int GetValidatedMenuChoice();
// ---- SQLite Function prototypes ----
sqlite3 *OpenDatabase(const string &path);
void CloseDatabase(sqlite3 *db);
// ---- First Run prototypes ----
int CountUsers(sqlite3 *db);
bool CreateFirstAccount(sqlite3 *db);
bool VerifyLogin(sqlite3 *db, const string &username, const string &password);
// ---- CRUD Functionality prototypes ----
void AddClient(sqlite3 *db);
void DeleteClient(sqlite3 *db);
// ---- Reporting Functionality prototypes ----
void ReportClients(sqlite3 *db);

// main flow
// greeting -> user login -> main menu loop -> 0;
int main()
{
    cout << "Created by Antonio Diaz-Leyva" << endl;
    cout << "Hello! Welcome to my CS-499 Capstone Database Artifact\n";

    sqlite3 *db = OpenDatabase(DATABASE_FILE);
    if (db == nullptr)
    {
        return 1;
    }

    // First run: no accounts exist yet
    int userCount = CountUsers(db);
    if (userCount < 0)
    {
        cout << "Unable to read user accounts" << endl;
        CloseDatabase(db);
        return 1;
    }
    if (userCount == 0)
    {
        cout << "No user accounts found. Creating first account" << endl;
        if (!CreateFirstAccount(db))
        {
            cout << "Account setup was not complete, exiting program" << endl;
            CloseDatabase(db);
            return 1;
        }

        cout << "Account created, please log in." << endl;
    }

    // Login attempts
    int answer;
    int attempts = 0;
    bool authenticated = false;
    do
    {
        answer = CheckUserPermissionAccess(db);
        attempts++;
        if (answer != 1)
        {
            cout << "Invalid Username or Password. Please try again\n";
        }
        else
        {
            authenticated = true;
        }
    } while (!authenticated && attempts < MAX_LOGIN_ATTEMPTS);

    if (!authenticated)
    {
        cout << "Too many failed login attempts. Exiting for security reasons.\n";
        CloseDatabase(db); // exit db on failed auth
        return 1;
    }

    // Main menu loop
    int choice;
    do
    {

        // option output
        cout << endl;
        cout << "DISPLAY the client list (enter 1)" << endl;
        cout << "ADD a new client (enter 2)" << endl;
        cout << "CHANGE a client's choice (enter 3)" << endl;
        cout << "DELETE a client (enter 4)" << endl;
        cout << "REPORT clients by filter (enter 5)" << endl;
        cout << "Exit the program.. (enter 6)" << endl;
        cout << endl;

        // validate user choice
        choice = GetValidatedMenuChoice();

        // functional flow
        if (choice == MENU_DISPLAY)
        {
            DisplayInfo(db);
        }
        else if (choice == MENU_ADD)
        {
            AddClient(db);
        }
        else if (choice == MENU_CHANGE)
        {
            ChangeCustomerChoice(db);
        }
        else if (choice == MENU_DELETE)
        {
            DeleteClient(db);
        }
        else if (choice == MENU_REPORT)
        {
            ReportClients(db);
        }
        else if (choice != MENU_EXIT)
        {
            cout << "Please enter a valid option from the menu.\n";
        }
    } while (choice != MENU_EXIT); // exit case

    CloseDatabase(db); // close db on exit
    return 0;
}

static void ReportDbError(sqlite3 *db, const string &userMessage)
{
    cout << userMessage << endl;
    if (SHOW_DB_ERROR_DETAILS && db != nullptr)
    {
        cerr << "[sqlite] " << sqlite3_errmsg(db) << endl;
    }
}

static bool ExecuteSql(sqlite3 *db, const char *sql)
{
    char *errorText = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errorText) != SQLITE_OK)
    {
        if (SHOW_DB_ERROR_DETAILS && errorText != nullptr)
        {
            cerr << "[sqlite] " << errorText << endl;
        }
        sqlite3_free(errorText);
        return false;
    }

    return true;
}

static bool CreateTables(sqlite3 *db)
{
    const char *schema =
        "CREATE TABLE IF NOT EXISTS accounts ("
        "    account_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    client_id  INTEGER NOT NULL,"
        "    amount     INTEGER NOT NULL,"
        "    service    INTEGER NOT NULL CHECK (service IN (1, 2))"
        ");"
        "CREATE TABLE IF NOT EXISTS clients ("
        "    client_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name        TEXT    NOT NULL,"
        "    address     TEXT    NOT NULL,"
        "    state       TEXT    NOT NULL,"
        "    zip         TEXT    NOT NULL,"
        "    phone       TEXT    NOT NULL,"
        "    status      TEXT    NOT NULL,"
        "    create_date TEXT    NOT NULL,"
        "    updated_at  TEXT    NOT NULL,"
        "    service     INTEGER NOT NULL CHECK (service IN (1, 2))"
        ");"
        "CREATE TABLE IF NOT EXISTS orders ("
        "    order_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    client_id    INTEGER NOT NULL,"
        "    order_date   TEXT    NOT NULL,"
        "    status       TEXT    NOT NULL,"
        "    total_amount INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS users ("
        "    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    username      TEXT NOT NULL UNIQUE,"
        "    salt          TEXT NOT NULL,"
        "    password_hash TEXT NOT NULL"
        ");";

    return ExecuteSql(db, schema);
}

// PRAGMA user_version stored in database to prevent reseeding each run
// or if users are deleted
static int GetUserVersion(sqlite3 *db)
{
    sqlite3_stmt *stmt = nullptr;
    int version = -1;

    if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            version = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    return version;
}

// On a brand-new database file, inserts the five starter clients.
static bool SeedClientsOnFirstRun(sqlite3 *db)
{
    int version = GetUserVersion(db);
    if (version < 0)
    {
        return false;
    }
    if (version >= 1)
    {
        return true; // already seeded on an earlier run
    }

    struct SeedClient
    {
        const char *name;
        const char *address;
        const char *state;
        const char *zip;
        const char *phone;
        int service; // 1 = Brokerage, 2 = Retirement
    };
    // Sample data only; replace with your own values.
    const SeedClient seeds[] = {
        {"Bob Jones", "12 Maple St", "NH", "03101", "555-0101", 1},
        {"Sarah Davis", "48 Oak Ave", "MA", "02108", "555-0102", 2},
        {"Amy Friendly", "7 Pine Rd", "ME", "04101", "555-0103", 1},
        {"Johnny Smith", "90 Birch Ln", "VT", "05401", "555-0104", 1},
        {"Carol Spears", "215 Cedar Ct", "CT", "06103", "555-0105", 2}};

    // One transaction: either all five clients and the version marker are
    // saved, or none of them are.
    if (!ExecuteSql(db, "BEGIN TRANSACTION;"))
    {
        return false;
    }

    sqlite3_stmt *insertStmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO clients (name, address, state, zip, phone, status, create_date, updated_at, service) "
                           "VALUES (?, ?, ?, ?, ?, 'active', datetime('now'), datetime('now'), ?);",
                           -1, &insertStmt, nullptr) != SQLITE_OK)
    {
        ExecuteSql(db, "ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (const SeedClient &seed : seeds)
    {
        sqlite3_bind_text(insertStmt, 1, seed.name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 2, seed.address, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 3, seed.state, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 4, seed.zip, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 5, seed.phone, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insertStmt, 6, seed.service);
        if (sqlite3_step(insertStmt) != SQLITE_DONE)
        {
            ok = false;
            break;
        }
        sqlite3_reset(insertStmt);
    }
    sqlite3_finalize(insertStmt);

    if (!ok || !ExecuteSql(db, "PRAGMA user_version = 1;"))
    {
        ExecuteSql(db, "ROLLBACK;");
        return false;
    }
    return ExecuteSql(db, "COMMIT;");
}

// Opens (or creates) the database file, makes sure both tables exist and
// seeds the starter clients on first run. Returns nullptr if anything fails.
sqlite3 *OpenDatabase(const string &path)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
    {
        ReportDbError(db, "Unable to open the client database.");
        sqlite3_close(db); // safe even when db is nullptr
        return nullptr;
    }
    if (!CreateTables(db) || !SeedClientsOnFirstRun(db))
    {
        ReportDbError(db, "Unable to prepare the client database.");
        sqlite3_close(db);
        return nullptr;
    }
    return db;
}

void CloseDatabase(sqlite3 *db)
{
    if (db != nullptr)
    {
        sqlite3_close(db);
    }
}

// ---------------------------------------------------------------------
// User accounts (users table)
// ---------------------------------------------------------------------

// Number of rows in users, or -1 if the query fails.
int CountUsers(sqlite3 *db) {
    sqlite3_stmt *stmt = nullptr;
    int count = -1;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    return count;
}

// Adds a user. password_hash holds the std::hash value as text. The salt
// column stays empty until the SHA-256 upgrade, so no schema change is
// needed then.
static bool InsertUser(sqlite3 *db, const string &name, const string &password) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO users (username, salt, password_hash) VALUES (?, '', ?);",
            -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    string hashText = to_string(HashPassword(password));
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashText.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Prompts for a username and a confirmed password, then saves the account.
// Returns false if input ends, the attempts run out or the save fails.
bool CreateFirstAccount(sqlite3 *db) {
    string newUsername;
    bool usernameOk = false;

    for (int attempt = 0; attempt < MAX_LOGIN_ATTEMPTS && !usernameOk; attempt++) {
        cout << "Choose a username (up to " << MAX_USERNAME_LENGTH << " characters, no spaces): \n";
        if (!(cin >> newUsername)) {
            return false;   // input closed or unreadable
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (newUsername.size() > MAX_USERNAME_LENGTH) {
            cout << "That username is too long.\n";
        } else {
            usernameOk = true;
        }
    }
    if (!usernameOk) {
        return false;
    }

    for (int attempt = 0; attempt < MAX_LOGIN_ATTEMPTS; attempt++) {
        cout << "Choose a password (at least " << MIN_PASSWORD_LENGTH << " characters): \n";
        string first = GetMaskedPassword();
        if (first.size() < MIN_PASSWORD_LENGTH) {
            cout << "That password is too short.\n";
            continue;
        }

        cout << "Re-enter the password to confirm: \n";
        string second = GetMaskedPassword();
        if (first != second) {
            cout << "The passwords did not match.\n";
            continue;
        }

        if (!InsertUser(db, newUsername, first)) {
            ReportDbError(db, "Unable to save the new account.");
            return false;
        }
        return true;
    }
    return false;
}

// True when the username exists and the password hashes to the stored value.
bool VerifyLogin(sqlite3 *db, const string &username, const string &password) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT password_hash FROM users WHERE username = ?;",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool match = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *stored = sqlite3_column_text(stmt, 0);
        if (stored != nullptr) {
            match = (to_string(HashPassword(password)) == reinterpret_cast<const char *>(stored));
        }
    }

    sqlite3_finalize(stmt);
    return match;
}

// Reads an integer menu choice safely, re-prompting on bad input.
int GetValidatedMenuChoice()
{
    int value;
    while (!(cin >> value))
    {
        if (cin.eof())
        {
            return MENU_EXIT; // input closed, exit the menu
        }

        cout << "Invalid input. Please enter a number from the menu." << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    return value; // input closed, exit menu
}

// Simple (non-cryptographic) hash helper used to avoid storing/
// comparing the password in plaintext.
size_t HashPassword(const string &plainText)
{
    return hash<string>{}(plainText);
}

// keep for pulling masked password from console.
string GetMaskedPassword()
{
    string pwd = "";
#ifdef _WIN32
    char ch;
    while ((ch = _getch()) != '\r')
    {
        if (ch == '\b')
        {
            if (!pwd.empty())
            {
                pwd.pop_back();
                cout << "\b \b";
            }
        }
        else
        {
            pwd.push_back(ch);
            cout << '*';
        }
    }
    cout << endl;
#else
    termios oldTermAttrs, noEchoAttrs;
    tcgetattr(STDIN_FILENO, &oldTermAttrs);
    noEchoAttrs = oldTermAttrs;
    noEchoAttrs.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &noEchoAttrs);

    getline(cin, pwd);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermAttrs);
    cout << endl;
#endif
    return pwd;
}

int CheckUserPermissionAccess(sqlite3 *db)
{
    int access;

    cout << "Enter your username: \n";
    cin >> username;

    // clear newline before masked read
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter your password: \n";
    string enteredPassword = GetMaskedPassword();

    if (VerifyLogin(db, username, enteredPassword))
    {
        access = 1; // correct password
    }
    else
    {
        access = 2; // incorrect password
    }

    return access;
}

// Prints every client from the database, one line per row.
void DisplayInfo(sqlite3 *db)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "SELECT client_id, name, service FROM clients ORDER BY client_id;",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        ReportDbError(db, "Unable to read the client list.");
        return;
    }

    cout << endl;
    cout << "Client's Name | Service (1 = Brokerage, 2 = Retirement)" << endl;

    int rows = 0;
    int result;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int clientId = sqlite3_column_int(stmt, 0);
        const unsigned char *nameText = sqlite3_column_text(stmt, 1);
        int service = sqlite3_column_int(stmt, 2);

        cout << clientId << ". "
             << (nameText != nullptr ? reinterpret_cast<const char *>(nameText) : "")
             << " | Service:  " << service << endl;
        rows++;
    }

    if (result != SQLITE_DONE)
    {
        ReportDbError(db, "Unable to read the client list.");
    }
    else if (rows == 0)
    {
        cout << "No clients found." << endl;
    }

    sqlite3_finalize(stmt);
}

// Text for a service number (1 = Brokerage, 2 = Retirement).
static string ServiceName(int service)
{
    if (service == 1)
    {
        return "Brokerage";
    }
    if (service == 2)
    {
        return "Retirement";
    }
    return "Unknown";
}

// Looks up one client. Returns 1 if found (name and service are filled in),
// 0 if no client has that id, -1 if the query fails.
static int FindClient(sqlite3 *db, int clientId, string &name, int &service)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT name, service FROM clients WHERE client_id = ?;",
                           -1, &stmt, nullptr) != SQLITE_OK)
    {
        return -1;
    }
    sqlite3_bind_int(stmt, 1, clientId);

    int result = sqlite3_step(stmt);
    int found = 0;
    if (result == SQLITE_ROW)
    {
        const unsigned char *nameText = sqlite3_column_text(stmt, 0);
        name = (nameText != nullptr) ? reinterpret_cast<const char *>(nameText) : "";
        service = sqlite3_column_int(stmt, 1);
        found = 1;
    }
    else if (result != SQLITE_DONE)
    {
        found = -1;
    }

    sqlite3_finalize(stmt);
    return found;
}

// Changes one client's service selection in the database.
void ChangeCustomerChoice(sqlite3 *db)
{
    int clientId;
    int newService;

    cout << "Enter the number of the client that you wish to change\n";
    while (!(cin >> clientId) || clientId < 1)
    {
        if (cin.eof())
        {
            return; // input closed
        }
        cout << "Invalid client number. Please enter a positive number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    string name;
    int currentService = 0;
    int found = FindClient(db, clientId, name, currentService);
    if (found < 0)
    {
        ReportDbError(db, "Unable to read the client record.");
        return;
    }
    if (found == 0)
    {
        cout << "No client found with that number.\n";
        return;
    }

    cout << endl;
    cout << "Changing " << name << " (currently " << ServiceName(currentService) << ").\n";
    cout << "Please enter the client's new service choice (1 = Brokerage, 2 = Retirement)\n";
    while (!(cin >> newService) || (newService != 1 && newService != 2))
    {
        if (cin.eof())
        {
            return; // input closed
        }
        cout << "Invalid service choice. Please enter 1 (Brokerage) or 2 (Retirement).\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "UPDATE clients SET service = ?, updated_at = datetime('now') WHERE client_id = ?;",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        ReportDbError(db, "Unable to update the client.");
        return;
    }
    sqlite3_bind_int(stmt, 1, newService);
    sqlite3_bind_int(stmt, 2, clientId);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        ReportDbError(db, "Unable to update the client.");
    }
    else if (sqlite3_changes(db) == 0)
    {
        cout << "No client found with that number.\n"; // removed after the lookup above
    }
    else
    {
        cout << name << " is now set to " << ServiceName(newService) << ".\n";
    }
}

// ---------------------------------------------------------------------
// Add and delete clients
// ---------------------------------------------------------------------

// Removes spaces and tabs (and stray carriage returns) from both ends.
static string Trim(const string &text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos)
    {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static bool IsValidState(const string &text)
{
    return text.size() == 2 &&
           isalpha(static_cast<unsigned char>(text[0])) &&
           isalpha(static_cast<unsigned char>(text[1]));
}

static bool IsValidZip(const string &text)
{
    if (text.size() != 5)
    {
        return false;
    }
    for (char c : text)
    {
        if (!isdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    return true;
}

// At least 7 digits; spaces and ( ) - + . are allowed as separators.
static bool IsValidPhone(const string &text)
{
    int digits = 0;
    for (char c : text)
    {
        if (isdigit(static_cast<unsigned char>(c)))
        {
            digits++;
        }
        else if (string(" ()-+.").find(c) == string::npos)
        {
            return false;
        }
    }
    return digits >= 7;
}

// Asks until the user enters a non-blank value of at most maxLength characters
// that passes isValid (pass nullptr to skip that check). Returns false if input closes.
static bool PromptText(const string &label, size_t maxLength,
                       bool (*isValid)(const string &), const string &invalidMessage,
                       string &value)
{
    while (true)
    {
        cout << label << "\n";
        if (!getline(cin, value))
        {
            return false;
        }
        value = Trim(value);

        if (value.empty())
        {
            cout << "This field cannot be blank.\n";
        }
        else if (value.size() > maxLength)
        {
            cout << "Please keep it to " << maxLength << " characters or fewer.\n";
        }
        else if (isValid != nullptr && !isValid(value))
        {
            cout << invalidMessage << "\n";
        }
        else
        {
            return true;
        }
    }
}

// Asks for a new client's details and inserts the row.
void AddClient(sqlite3 *db)
{
    // finish the line left over from the menu choice
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string name, address, state, zip, phone;
    int service;

    if (!PromptText("Enter the client's name:", 100, nullptr, "", name) ||
        !PromptText("Enter the client's street address:", 100, nullptr, "", address) ||
        !PromptText("Enter the client's 2-letter state (for example NH):", 2,
                    IsValidState, "State must be exactly two letters.", state) ||
        !PromptText("Enter the client's 5-digit ZIP code:", 5,
                    IsValidZip, "ZIP code must be exactly 5 digits.", zip) ||
        !PromptText("Enter the client's phone number:", 20,
                    IsValidPhone, "Phone must have at least 7 digits (spaces and ( ) - + . are allowed).", phone))
    {
        return; // input closed
    }

    for (char &c : state)
    {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }

    cout << "Please enter the client's service choice (1 = Brokerage, 2 = Retirement)\n";
    while (!(cin >> service) || (service != 1 && service != 2))
    {
        if (cin.eof())
        {
            return; // input closed
        }
        cout << "Invalid service choice. Please enter 1 (Brokerage) or 2 (Retirement).\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO clients (name, address, state, zip, phone, status, create_date, updated_at, service) "
            "VALUES (?, ?, ?, ?, ?, 'active', datetime('now'), datetime('now'), ?);",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        ReportDbError(db, "Unable to add the client.");
        return;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, zip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, phone.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, service);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        ReportDbError(db, "Unable to add the client.");
    }
    else
    {
        cout << "Added " << name << " as client " << sqlite3_last_insert_rowid(db) << ".\n";
    }
}

// Runs a DELETE that takes one client id. Returns true if it ran without error.
static bool DeleteRowsForClient(sqlite3 *db, const char *sql, int clientId)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_int(stmt, 1, clientId);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Deletes a client after a confirmation. The client's accounts and orders are
// removed in the same transaction so no orphaned rows are left behind.
void DeleteClient(sqlite3 *db)
{
    int clientId;

    cout << "Enter the number of the client that you wish to delete\n";
    while (!(cin >> clientId) || clientId < 1)
    {
        if (cin.eof())
        {
            return; // input closed
        }
        cout << "Invalid client number. Please enter a positive number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    string name;
    int service = 0;
    int found = FindClient(db, clientId, name, service);
    if (found < 0)
    {
        ReportDbError(db, "Unable to read the client record.");
        return;
    }
    if (found == 0)
    {
        cout << "No client found with that number.\n";
        return;
    }

    cout << "Delete " << name << " (client " << clientId << ", " << ServiceName(service)
         << ")? This also removes their accounts and orders. Type Y to confirm:\n";
    string answer;
    if (!(cin >> answer))
    {
        return; // input closed
    }
    if (answer != "Y" && answer != "y")
    {
        cout << "Delete cancelled.\n";
        return;
    }

    if (!ExecuteSql(db, "BEGIN TRANSACTION;"))
    {
        ReportDbError(db, "Unable to delete the client.");
        return;
    }

    // the last statement is the clients delete, so sqlite3_changes() below
    // reports whether that row was removed
    bool ok = DeleteRowsForClient(db, "DELETE FROM accounts WHERE client_id = ?;", clientId) &&
              DeleteRowsForClient(db, "DELETE FROM orders WHERE client_id = ?;", clientId) &&
              DeleteRowsForClient(db, "DELETE FROM clients WHERE client_id = ?;", clientId);

    if (!ok || sqlite3_changes(db) != 1)
    {
        ExecuteSql(db, "ROLLBACK;");
        ReportDbError(db, "Unable to delete the client.");
        return;
    }
    if (!ExecuteSql(db, "COMMIT;"))
    {
        ExecuteSql(db, "ROLLBACK;");
        ReportDbError(db, "Unable to delete the client.");
        return;
    }

    cout << name << " was deleted.\n";
}

// ---------------------------------------------------------------------
// Filtered client list report
// ---------------------------------------------------------------------

// Escapes % _ and \ so a search term is matched literally by LIKE ... ESCAPE '\'.
static string EscapeLike(const string &text)
{
    string escaped;
    for (char c : text)
    {
        if (c == '%' || c == '_' || c == '\\')
        {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

// Prints the rows of a prepared client query (client_id, name, service)
// in the same layout as DisplayInfo, followed by a row count.
static void PrintClientReport(sqlite3 *db, sqlite3_stmt *stmt, const string &title)
{
    cout << "\n" << title << "\n";
    cout << "Client's Name | Service (1 = Brokerage, 2 = Retirement)" << endl;
    cout << endl;

    int rows = 0;
    int result;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int clientId = sqlite3_column_int(stmt, 0);
        const unsigned char *nameText = sqlite3_column_text(stmt, 1);
        int service = sqlite3_column_int(stmt, 2);

        cout << clientId << ". "
             << (nameText != nullptr ? reinterpret_cast<const char *>(nameText) : "")
             << " selected option " << service << endl;
        rows++;
    }

    if (result != SQLITE_DONE)
    {
        ReportDbError(db, "Unable to read the client list.");
    }
    else if (rows == 0)
    {
        cout << "No clients matched." << endl;
    }
    else
    {
        cout << rows << (rows == 1 ? " client listed." : " clients listed.") << endl;
    }
}

// Lists clients filtered by service, by part of the name, or all of them.
void ReportClients(sqlite3 *db)
{
    cout << endl;
    cout << "Client report - choose a filter:\n";
    cout << "  1. By service (Brokerage or Retirement)\n";
    cout << "  2. By name (search text)\n";
    cout << "  3. All clients\n";
    cout << endl;

    int filter;
    while (!(cin >> filter) || filter < 1 || filter > 3)
    {
        if (cin.eof())
        {
            return; // input closed
        }
        cout << "Invalid choice. Please enter 1, 2, or 3.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // finish the line

    sqlite3_stmt *stmt = nullptr;
    string title;

    if (filter == 1)
    {
        int service;
        cout << "Enter the service to list (1 = Brokerage, 2 = Retirement)\n";
        while (!(cin >> service) || (service != 1 && service != 2))
        {
            if (cin.eof())
            {
                return; // input closed
            }
            cout << "Invalid service choice. Please enter 1 (Brokerage) or 2 (Retirement).\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        if (sqlite3_prepare_v2(db,
                "SELECT client_id, name, service FROM clients WHERE service = ? ORDER BY name, client_id;",
                -1, &stmt, nullptr) != SQLITE_OK)
        {
            ReportDbError(db, "Unable to read the client list.");
            return;
        }
        sqlite3_bind_int(stmt, 1, service);
        title = "Clients with " + ServiceName(service) + ":";
    }
    else if (filter == 2)
    {
        string search;
        if (!PromptText("Enter part of the client's name:", 100, nullptr, "", search))
        {
            return; // input closed
        }

        if (sqlite3_prepare_v2(db,
                "SELECT client_id, name, service FROM clients "
                "WHERE name LIKE ? ESCAPE '\\' ORDER BY name, client_id;",
                -1, &stmt, nullptr) != SQLITE_OK)
        {
            ReportDbError(db, "Unable to read the client list.");
            return;
        }
        string pattern = "%" + EscapeLike(search) + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        title = "Clients with names containing \"" + search + "\":";
    }
    else
    {
        if (sqlite3_prepare_v2(db,
                "SELECT client_id, name, service FROM clients ORDER BY name, client_id;",
                -1, &stmt, nullptr) != SQLITE_OK)
        {
            ReportDbError(db, "Unable to read the client list.");
            return;
        }
        title = "All clients:";
    }

    PrintClientReport(db, stmt, title);
    sqlite3_finalize(stmt);
}