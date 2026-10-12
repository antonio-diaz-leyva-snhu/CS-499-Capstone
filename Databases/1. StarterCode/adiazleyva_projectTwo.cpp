// =====================================================================
// SNHU Investments - Client Management Application
// Project Two: Security Vulnerability Review and Fix Implementations
// Author: Antonio Diaz-Leyva
// =====================================================================

#include <iostream>
#include <string>
#include <limits>
#include <functional>

// Cross-platform helper so the password is not echoed to the screen
// while it is typed (see SECURITY FIX #3 below).
#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

using namespace std;

// ---------------------------------------------------------------------
// SECURITY VULNERABILITY #1: Hardcoded / weak plaintext password.

// SECURITY FIX #1: The raw plaintext password is no longer stored as a
// global variable. Instead, only a hash of the password is kept
// (storedPasswordHash, set up in main()), and the char[16] buffer was
// removed entirely. NOTE: std::hash is NOT a cryptographically secure
// hash function, this is just used as an example for the concept of 
// hashing.

// ---- Global variables ----
// SECURITY VULNERABILITY #2: Excessive use of global variables.

// SECURITY FIX #2 (partial): Variables that only matter within a
// single operation (changechoice, newservice, per-attempt password
// entry) have been moved to local/function scope below so they can no
// longer be read or altered from unrelated code. 
string username;

// SECURITY VULNERABILITY #3: Fixed-size C-style char buffers.
//
// SECURITY FIX #3: The fixed-size char arrays were replaced with
// std::string, which manages its own memory and cannot be overflowed
// by normal string operations.
string name1 = "Bob Jones";
string name2 = "Sarah Davis";
string name3 = "Amy Friendly";
string name4 = "Johnny Smith";
string name5 = "Carol Spears";

int num1 = 1;
int num2 = 2;
int num3 = 1;
int num4 = 1;
int num5 = 2;

// Hash of the valid password, computed once in main(). See FIX #1.
size_t storedPasswordHash = 0;

const int MAX_LOGIN_ATTEMPTS = 3; // used by SECURITY FIX #4 below

// ---- Function prototypes ----
int CheckUserPermissionAccess();
void DisplayInfo();
void ChangeCustomerChoice();
string GetMaskedPassword();
size_t HashPassword(const string &plainText);
int GetValidatedMenuChoice();

int main() {
    cout << "Created by Antonio Diaz-Leyva" << endl;
    cout << "Hello! Welcome to our Investment Company\n";

    // Set up the "stored" credential as a hash instead of plaintext.
    // See SECURITY VULNERABILITY #1 / SECURITY FIX #1 above.
    storedPasswordHash = HashPassword("123");

    // -----------------------------------------------------------------
    // SECURITY VULNERABILITY #4: Unlimited login attempts.
    //
    // SECURITY FIX #4: A maximum attempt counter (MAX_LOGIN_ATTEMPTS)
    // was added. After 3 failed attempts the program exits instead of
    // continuing to prompt, which mitigates brute-force guessing from
    // this interface.
    int answer;
    int attempts = 0;
    bool authenticated = false;
    do {
        answer = CheckUserPermissionAccess();
        attempts++;
        if (answer != 1) {
            cout << "Invalid Password. Please try again\n";
        } else {
            authenticated = true;
        }
    } while (!authenticated && attempts < MAX_LOGIN_ATTEMPTS);

    if (!authenticated) {
        cout << "Too many failed login attempts. Exiting for security reasons.\n";
        return 1;
    }

    // Main menu loop
    int choice;
    do {
        cout << "What would you like to do?\n";
        cout << "DISPLAY the client list (enter 1)\n";
        cout << "CHANGE a client's choice (enter 2)\n";
        cout << "Exit the program.. (enter 3)\n";

        // -------------------------------------------------------------
        // SECURITY VULNERABILITY #5: No input validation on menu choice.
        //
        // SECURITY FIX #5: GetValidatedMenuChoice() checks cin's fail
        // state, clears it, and discards bad input before re-prompting,
        // so malformed input can no longer desync the input stream.
        // -------------------------------------------------------------
        choice = GetValidatedMenuChoice();
        cout << "You chose " << choice << endl;

        if (choice == 1) {
            DisplayInfo();
        } else if (choice == 2) {
            ChangeCustomerChoice();
        } else if (choice != 3) {
            cout << "Please enter a valid option (1, 2, or 3).\n";
        }
    } while (choice != 3);

    return 0;
}

// Reads an integer menu choice safely, re-prompting on bad input.
// Part of SECURITY FIX #5.
int GetValidatedMenuChoice() {
    int value;
    while (!(cin >> value)) {
        cout << "Invalid input. Please enter a number (1, 2, or 3).\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    return value;
}

// Simple (non-cryptographic) hash helper used to avoid storing/
// comparing the password in plaintext. See SECURITY FIX #1.
size_t HashPassword(const string &plainText) {
    return hash<string>{}(plainText);
}

// -----------------------------------------------------------------------
// SECURITY VULNERABILITY #6 (in GetMaskedPassword / original CheckUser
// PermissionAccess): The password was read with "cin >> enteredPassword;"
// which echoes every typed character to the screen in plain view
//
// SECURITY FIX #6: GetMaskedPassword() disables terminal echo while the
// password is entered and prints '*' for each character instead, so the
// real characters are never shown on screen.
// -----------------------------------------------------------------------
string GetMaskedPassword() {
    string pwd = "";
#ifdef _WIN32
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b') {
            if (!pwd.empty()) {
                pwd.pop_back();
                cout << "\b \b";
            }
        } else {
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

int CheckUserPermissionAccess() {
    int access;

    cout << "Enter your username: \n";
    cin >> username;
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // clear newline before masked read

    cout << "Enter your password: \n";
    string enteredPassword = GetMaskedPassword(); // SECURITY FIX #6

    // -------------------------------------------------------------
    // SECURITY VULNERABILITY #7: Password compared against a
    // hardcoded plaintext literal ("123"), and the username entered
    // above was never checked against anything 
    //
    // SECURITY FIX #7: The comparison now hashes the entered
    // password and compares hash values, so the real password is
    // never held in a plaintext comparison in this function. Full
    // per-user authentication / role-based access control would
    // require a real user-account store (e.g., a database of
    // users, hashed passwords, and roles) whichs seems beyond
	// scope of the project
    size_t enteredHash = HashPassword(enteredPassword);

    if (enteredHash == storedPasswordHash) {
        access = 1;   // correct password
    } else {
        access = 2;   // incorrect password
    }

    return access;
}

void DisplayInfo() {
    cout << "  Client's Name    Service Selected (1 = Brokerage, 2 = Retirement)" << endl;
    cout << "1. " << name1 << " selected option " << num1 << endl;
    cout << "2. " << name2 << " selected option " << num2 << endl;
    cout << "3. " << name3 << " selected option " << num3 << endl;
    cout << "4. " << name4 << " selected option " << num4 << endl;
    cout << "5. " << name5 << " selected option " << num5 << endl;
}

void ChangeCustomerChoice() {
    // SECURITY VULNERABILITY #8: No validation on user-supplied indices
    // or values. 
    //
    // SECURITY FIX #8: Both changechoice and newservice are now
    // validated (with re-prompting / an error message) before being
    // applied, and these variables were moved out of global scope into
    // this function's local scope so they cannot be read or modified
    // by unrelated code elsewhere in the program.
	
    int changechoice;
    int newservice;

    cout << "Enter the number of the client that you wish to change\n";
    while (!(cin >> changechoice) || changechoice < 1 || changechoice > 5) {
        cout << "Invalid client number. Please enter a number 1-5.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    cout << "Please enter the client's new service choice (1 = Brokerage, 2 = Retirement)\n";
    while (!(cin >> newservice) || (newservice != 1 && newservice != 2)) {
        cout << "Invalid service choice. Please enter 1 (Brokerage) or 2 (Retirement).\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    if (changechoice == 1) {
        num1 = newservice;
    } else if (changechoice == 2) {
        num2 = newservice;
    } else if (changechoice == 3) {
        num3 = newservice;
    } else if (changechoice == 4) {
        num4 = newservice;
    } else if (changechoice == 5) {
        num5 = newservice;
    }
}
