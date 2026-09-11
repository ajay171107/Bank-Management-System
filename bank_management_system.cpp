/*
 * ============================================================
 *   BANK MANAGEMENT SYSTEM
 *   A console-based, file-persisted banking application in C++
 * ============================================================
 *
 * Features:
 *   - Create new accounts (auto-generated account numbers)
 *   - PIN-protected login
 *   - Deposit / Withdraw money
 *   - Transfer money between accounts
 *   - Balance enquiry
 *   - View / modify account details
 *   - Delete an account
 *   - View all accounts (admin)
 *   - Per-account transaction history (persisted to disk)
 *   - All data persisted to disk using binary file I/O, so data
 *     survives across program runs.
 *
 * Compile:
 *   g++ -std=c++17 -O2 -o bank bank_management_system.cpp
 *
 * Run:
 *   ./bank
 *
 * Data files created in the working directory:
 *   accounts.dat      -> account records (binary)
 *   transactions.dat  -> transaction log records (binary)
 */

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <vector>
#include <limits>
#include <ctime>
#include <algorithm>

using namespace std;

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------
const string ACCOUNTS_FILE     = "accounts.dat";
const string TRANSACTIONS_FILE = "transactions.dat";
const double MIN_BALANCE       = 500.0;   // minimum balance to keep account open
const int    ADMIN_PIN         = 9999;    // simple admin PIN for admin menu

// ------------------------------------------------------------
// Utility: clear input stream on bad input
// ------------------------------------------------------------
void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int readInt(const string &prompt) {
    int value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail()) {
            clearInput();
            cout << "  Invalid input. Please enter a valid number.\n";
        } else {
            clearInput();
            return value;
        }
    }
}

double readDouble(const string &prompt) {
    double value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail()) {
            clearInput();
            cout << "  Invalid input. Please enter a valid amount.\n";
        } else {
            clearInput();
            return value;
        }
    }
}

string currentTimestamp() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// ------------------------------------------------------------
// Account record (fixed-size for simple binary file storage)
// ------------------------------------------------------------
struct Account {
    int    accountNumber;
    char   name[50];
    char   address[100];
    char   phone[20];
    int    pin;
    char   accountType[15]; // "Savings" or "Current"
    double balance;
    bool   active; // false = deleted/closed

    Account() {
        accountNumber = 0;
        memset(name, 0, sizeof(name));
        memset(address, 0, sizeof(address));
        memset(phone, 0, sizeof(phone));
        pin = 0;
        memset(accountType, 0, sizeof(accountType));
        balance = 0.0;
        active = true;
    }
};

// ------------------------------------------------------------
// Transaction record
// ------------------------------------------------------------
struct Transaction {
    int    accountNumber;
    char   type[20];   // Deposit, Withdraw, Transfer-Out, Transfer-In, Open
    double amount;
    double balanceAfter;
    char   timestamp[32];
    int    relatedAccount; // for transfers, 0 otherwise

    Transaction() {
        accountNumber = 0;
        memset(type, 0, sizeof(type));
        amount = 0.0;
        balanceAfter = 0.0;
        memset(timestamp, 0, sizeof(timestamp));
        relatedAccount = 0;
    }
};

// ------------------------------------------------------------
// Bank class: handles all file I/O and business logic
// ------------------------------------------------------------
class Bank {
public:
    // ---------- Account existence / lookup ----------
    bool readAccount(int accNo, Account &acc) {
        fstream file(ACCOUNTS_FILE, ios::in | ios::binary);
        if (!file) return false;
        while (file.read(reinterpret_cast<char*>(&acc), sizeof(Account))) {
            if (acc.accountNumber == accNo && acc.active) {
                file.close();
                return true;
            }
        }
        file.close();
        return false;
    }

    bool writeAccountUpdate(const Account &acc) {
        fstream file(ACCOUNTS_FILE, ios::in | ios::out | ios::binary);
        if (!file) return false;
        Account temp;
        while (file.read(reinterpret_cast<char*>(&temp), sizeof(Account))) {
            if (temp.accountNumber == acc.accountNumber) {
                streampos pos = file.tellg();
                pos -= static_cast<streamoff>(sizeof(Account));
                file.seekp(pos);
                file.write(reinterpret_cast<const char*>(&acc), sizeof(Account));
                file.close();
                return true;
            }
        }
        file.close();
        return false;
    }

    int getNextAccountNumber() {
        fstream file(ACCOUNTS_FILE, ios::in | ios::binary);
        int maxAcc = 1000; // accounts start at 1001
        Account temp;
        if (file) {
            while (file.read(reinterpret_cast<char*>(&temp), sizeof(Account))) {
                if (temp.accountNumber > maxAcc) maxAcc = temp.accountNumber;
            }
        }
        file.close();
        return maxAcc + 1;
    }

    void logTransaction(int accNo, const string &type, double amount,
                         double balanceAfter, int relatedAccount = 0) {
        Transaction t;
        t.accountNumber = accNo;
        strncpy(t.type, type.c_str(), sizeof(t.type) - 1);
        t.amount = amount;
        t.balanceAfter = balanceAfter;
        string ts = currentTimestamp();
        strncpy(t.timestamp, ts.c_str(), sizeof(t.timestamp) - 1);
        t.relatedAccount = relatedAccount;

        ofstream file(TRANSACTIONS_FILE, ios::out | ios::binary | ios::app);
        file.write(reinterpret_cast<const char*>(&t), sizeof(Transaction));
        file.close();
    }

    // ---------- Core operations ----------
    void createAccount() {
        Account acc;
        string input;

        cout << "\n----- OPEN NEW ACCOUNT -----\n";
        cout << "Enter full name: ";
        getline(cin, input);
        strncpy(acc.name, input.c_str(), sizeof(acc.name) - 1);

        cout << "Enter address: ";
        getline(cin, input);
        strncpy(acc.address, input.c_str(), sizeof(acc.address) - 1);

        cout << "Enter phone number: ";
        getline(cin, input);
        strncpy(acc.phone, input.c_str(), sizeof(acc.phone) - 1);

        int typeChoice;
        cout << "Account type: 1) Savings  2) Current\nChoice: ";
        cin >> typeChoice;
        clearInput();
        string typeStr = (typeChoice == 2) ? "Current" : "Savings";
        strncpy(acc.accountType, typeStr.c_str(), sizeof(acc.accountType) - 1);

        while (true) {
            acc.pin = readInt("Set a 4-digit PIN: ");
            if (acc.pin >= 1000 && acc.pin <= 9999) break;
            cout << "  PIN must be exactly 4 digits (1000-9999).\n";
        }

        double initialDeposit = readDouble("Enter initial deposit (min " + to_string((int)MIN_BALANCE) + "): ");
        if (initialDeposit < MIN_BALANCE) {
            cout << "  Initial deposit must be at least " << MIN_BALANCE << ". Account not created.\n";
            return;
        }
        acc.balance = initialDeposit;
        acc.accountNumber = getNextAccountNumber();
        acc.active = true;

        ofstream file(ACCOUNTS_FILE, ios::out | ios::binary | ios::app);
        file.write(reinterpret_cast<const char*>(&acc), sizeof(Account));
        file.close();

        logTransaction(acc.accountNumber, "Open", acc.balance, acc.balance);

        cout << "\n  Account created successfully!\n";
        cout << "  Your account number is: " << acc.accountNumber << "\n";
        cout << "  Please note this down; you'll need it along with your PIN to log in.\n";
    }

    bool authenticate(int accNo, int pin, Account &acc) {
        if (!readAccount(accNo, acc)) return false;
        return acc.pin == pin;
    }

    void deposit(Account &acc) {
        double amount = readDouble("Enter amount to deposit: ");
        if (amount <= 0) {
            cout << "  Amount must be positive.\n";
            return;
        }
        acc.balance += amount;
        writeAccountUpdate(acc);
        logTransaction(acc.accountNumber, "Deposit", amount, acc.balance);
        cout << fixed << setprecision(2);
        cout << "  Deposit successful. New balance: " << acc.balance << "\n";
    }

    void withdraw(Account &acc) {
        double amount = readDouble("Enter amount to withdraw: ");
        if (amount <= 0) {
            cout << "  Amount must be positive.\n";
            return;
        }
        if (acc.balance - amount < MIN_BALANCE) {
            cout << "  Insufficient balance. Minimum balance of " << MIN_BALANCE << " must be maintained.\n";
            return;
        }
        acc.balance -= amount;
        writeAccountUpdate(acc);
        logTransaction(acc.accountNumber, "Withdraw", amount, acc.balance);
        cout << fixed << setprecision(2);
        cout << "  Withdrawal successful. New balance: " << acc.balance << "\n";
    }

    void transfer(Account &acc) {
        int destAcc = readInt("Enter destination account number: ");
        if (destAcc == acc.accountNumber) {
            cout << "  Cannot transfer to the same account.\n";
            return;
        }
        Account dest;
        if (!readAccount(destAcc, dest)) {
            cout << "  Destination account not found.\n";
            return;
        }
        double amount = readDouble("Enter amount to transfer: ");
        if (amount <= 0) {
            cout << "  Amount must be positive.\n";
            return;
        }
        if (acc.balance - amount < MIN_BALANCE) {
            cout << "  Insufficient balance. Minimum balance of " << MIN_BALANCE << " must be maintained.\n";
            return;
        }
        acc.balance -= amount;
        dest.balance += amount;
        writeAccountUpdate(acc);
        writeAccountUpdate(dest);
        logTransaction(acc.accountNumber, "Transfer-Out", amount, acc.balance, dest.accountNumber);
        logTransaction(dest.accountNumber, "Transfer-In", amount, dest.balance, acc.accountNumber);
        cout << fixed << setprecision(2);
        cout << "  Transfer successful. New balance: " << acc.balance << "\n";
    }

    void showBalance(const Account &acc) {
        cout << fixed << setprecision(2);
        cout << "\n  Account Number : " << acc.accountNumber << "\n";
        cout << "  Name           : " << acc.name << "\n";
        cout << "  Account Type   : " << acc.accountType << "\n";
        cout << "  Current Balance: " << acc.balance << "\n";
    }

    void showAccountDetails(const Account &acc) {
        cout << fixed << setprecision(2);
        cout << "\n--------- ACCOUNT DETAILS ---------\n";
        cout << "  Account Number : " << acc.accountNumber << "\n";
        cout << "  Name           : " << acc.name << "\n";
        cout << "  Address        : " << acc.address << "\n";
        cout << "  Phone          : " << acc.phone << "\n";
        cout << "  Account Type   : " << acc.accountType << "\n";
        cout << "  Balance        : " << acc.balance << "\n";
        cout << "------------------------------------\n";
    }

    void modifyAccount(Account &acc) {
        cout << "\n----- MODIFY ACCOUNT DETAILS -----\n";
        cout << "(Press Enter to keep current value)\n";
        string input;

        cout << "Name [" << acc.name << "]: ";
        getline(cin, input);
        if (!input.empty()) strncpy(acc.name, input.c_str(), sizeof(acc.name) - 1);

        cout << "Address [" << acc.address << "]: ";
        getline(cin, input);
        if (!input.empty()) strncpy(acc.address, input.c_str(), sizeof(acc.address) - 1);

        cout << "Phone [" << acc.phone << "]: ";
        getline(cin, input);
        if (!input.empty()) strncpy(acc.phone, input.c_str(), sizeof(acc.phone) - 1);

        cout << "Change PIN? (y/n): ";
        getline(cin, input);
        if (!input.empty() && (input[0] == 'y' || input[0] == 'Y')) {
            while (true) {
                int newPin = readInt("Enter new 4-digit PIN: ");
                if (newPin >= 1000 && newPin <= 9999) { acc.pin = newPin; break; }
                cout << "  PIN must be exactly 4 digits.\n";
            }
        }

        writeAccountUpdate(acc);
        cout << "  Account details updated successfully.\n";
    }

    void deleteAccount(Account &acc) {
        cout << "\nAre you sure you want to close this account? All funds ("
             << fixed << setprecision(2) << acc.balance
             << ") must be withdrawn first, or they will be forfeited.\n";
        cout << "Type 'CONFIRM' to proceed: ";
        string confirm;
        getline(cin, confirm);
        if (confirm != "CONFIRM") {
            cout << "  Account closure cancelled.\n";
            return;
        }
        acc.active = false;
        writeAccountUpdate(acc);
        logTransaction(acc.accountNumber, "Close", acc.balance, 0.0);
        cout << "  Account closed successfully.\n";
    }

    void showTransactionHistory(int accNo) {
        ifstream file(TRANSACTIONS_FILE, ios::in | ios::binary);
        if (!file) {
            cout << "  No transaction history found.\n";
            return;
        }
        Transaction t;
        vector<Transaction> history;
        while (file.read(reinterpret_cast<char*>(&t), sizeof(Transaction))) {
            if (t.accountNumber == accNo) history.push_back(t);
        }
        file.close();

        if (history.empty()) {
            cout << "  No transactions found for this account.\n";
            return;
        }

        cout << fixed << setprecision(2);
        cout << "\n----------------------------------------------------------------------\n";
        cout << left << setw(20) << "Date/Time" << setw(15) << "Type"
             << setw(12) << "Amount" << setw(15) << "Balance After" << "Related Acc\n";
        cout << "----------------------------------------------------------------------\n";
        for (auto &tx : history) {
            cout << left << setw(20) << tx.timestamp << setw(15) << tx.type
                 << setw(12) << tx.amount << setw(15) << tx.balanceAfter
                 << (tx.relatedAccount ? to_string(tx.relatedAccount) : string("-")) << "\n";
        }
        cout << "----------------------------------------------------------------------\n";
    }

    // ---------- Admin operations ----------
    void showAllAccounts() {
        ifstream file(ACCOUNTS_FILE, ios::in | ios::binary);
        if (!file) {
            cout << "  No accounts found.\n";
            return;
        }
        Account acc;
        bool any = false;
        cout << fixed << setprecision(2);
        cout << "\n--------------------------------------------------------------------------\n";
        cout << left << setw(10) << "AccNo" << setw(22) << "Name" << setw(12)
             << "Type" << setw(14) << "Balance" << "Status\n";
        cout << "--------------------------------------------------------------------------\n";
        while (file.read(reinterpret_cast<char*>(&acc), sizeof(Account))) {
            any = true;
            cout << left << setw(10) << acc.accountNumber << setw(22) << acc.name
                 << setw(12) << acc.accountType << setw(14) << acc.balance
                 << (acc.active ? "Active" : "Closed") << "\n";
        }
        file.close();
        if (!any) cout << "  (no records)\n";
        cout << "--------------------------------------------------------------------------\n";
    }

    int countActiveAccounts() {
        ifstream file(ACCOUNTS_FILE, ios::in | ios::binary);
        int count = 0;
        Account acc;
        if (file) {
            while (file.read(reinterpret_cast<char*>(&acc), sizeof(Account))) {
                if (acc.active) count++;
            }
        }
        return count;
    }

    double totalBankFunds() {
        ifstream file(ACCOUNTS_FILE, ios::in | ios::binary);
        double total = 0.0;
        Account acc;
        if (file) {
            while (file.read(reinterpret_cast<char*>(&acc), sizeof(Account))) {
                if (acc.active) total += acc.balance;
            }
        }
        return total;
    }
};

// ------------------------------------------------------------
// Menus
// ------------------------------------------------------------
void printHeader(const string &title) {
    cout << "\n============================================\n";
    cout << "   " << title << "\n";
    cout << "============================================\n";
}

void customerMenu(Bank &bank, Account &acc) {
    while (true) {
        printHeader("WELCOME, " + string(acc.name));
        cout << "1. Check Balance\n";
        cout << "2. Deposit Money\n";
        cout << "3. Withdraw Money\n";
        cout << "4. Transfer Money\n";
        cout << "5. View Account Details\n";
        cout << "6. Modify Account Details\n";
        cout << "7. Transaction History\n";
        cout << "8. Close Account\n";
        cout << "9. Logout\n";
        int choice = readInt("Enter your choice: ");

        // Refresh account data each loop in case of external changes
        Account fresh;
        if (choice != 9 && (!bank.readAccount(acc.accountNumber, fresh))) {
            cout << "  This account has been closed. Logging out.\n";
            return;
        }
        if (choice != 9) acc = fresh;

        switch (choice) {
            case 1: bank.showBalance(acc); break;
            case 2: bank.deposit(acc); break;
            case 3: bank.withdraw(acc); break;
            case 4: bank.transfer(acc); break;
            case 5: bank.showAccountDetails(acc); break;
            case 6: bank.modifyAccount(acc); break;
            case 7: bank.showTransactionHistory(acc.accountNumber); break;
            case 8:
                bank.deleteAccount(acc);
                if (!acc.active) return;
                break;
            case 9:
                cout << "  Logged out. Thank you for banking with us!\n";
                return;
            default:
                cout << "  Invalid choice. Try again.\n";
        }
    }
}

void adminMenu(Bank &bank) {
    while (true) {
        printHeader("ADMIN PANEL");
        cout << "1. View All Accounts\n";
        cout << "2. Total Active Accounts\n";
        cout << "3. Total Bank Funds\n";
        cout << "4. View Account Transaction History\n";
        cout << "5. Back to Main Menu\n";
        int choice = readInt("Enter your choice: ");

        switch (choice) {
            case 1: bank.showAllAccounts(); break;
            case 2: cout << "  Total active accounts: " << bank.countActiveAccounts() << "\n"; break;
            case 3: cout << fixed << setprecision(2)
                          << "  Total funds held by the bank: " << bank.totalBankFunds() << "\n"; break;
            case 4: {
                int accNo = readInt("Enter account number: ");
                bank.showTransactionHistory(accNo);
                break;
            }
            case 5: return;
            default: cout << "  Invalid choice. Try again.\n";
        }
    }
}

void loginFlow(Bank &bank) {
    int accNo = readInt("Enter your account number: ");
    int pin = readInt("Enter your 4-digit PIN: ");
    Account acc;
    if (bank.authenticate(accNo, pin, acc)) {
        cout << "\n  Login successful.\n";
        customerMenu(bank, acc);
    } else {
        cout << "\n  Invalid account number or PIN.\n";
    }
}

int main() {
    Bank bank;

    while (true) {
        printHeader("BANK MANAGEMENT SYSTEM");
        cout << "1. Create New Account\n";
        cout << "2. Login to Existing Account\n";
        cout << "3. Admin Panel\n";
        cout << "4. Exit\n";
        int choice = readInt("Enter your choice: ");

        switch (choice) {
            case 1:
                bank.createAccount();
                break;
            case 2:
                loginFlow(bank);
                break;
            case 3: {
                int pin = readInt("Enter admin PIN: ");
                if (pin == ADMIN_PIN) {
                    adminMenu(bank);
                } else {
                    cout << "  Incorrect admin PIN.\n";
                }
                break;
            }
            case 4:
                cout << "\n  Thank you for using the Bank Management System. Goodbye!\n";
                return 0;
            default:
                cout << "  Invalid choice. Please try again.\n";
        }
    }

    return 0;
}
