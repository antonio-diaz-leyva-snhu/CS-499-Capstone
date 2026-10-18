// =====================================================================
// SNHU Investments - Client Management Application
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


string username;

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
