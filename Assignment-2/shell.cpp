#include <bits/stdc++.h>
using namespace std;
#define MAXCMDLEN 500
#define CMDNAME 100
#define CMDARGS 30
#define CMDNUMPIPE 50

class Command {
public:
    string enteredCmd;
    string cmdName;
    bool isPipe;
    bool isBackground;
    string inFile;
    string outFile;
    vector<Command> pipeCmds;
    Command(string _enteredCmd) {
        bool readSuccess = parseCommand(_enteredCmd, this);
    }
};

void trim(string str) {

}

bool parseCommand(string enteredcmd, Command* cmd) {
    // parse command and extract all the information
    trim(enteredcmd);
    int len = enteredcmd.length();
    cmd->isBackground = (enteredcmd[len-1] == '&');
    
}

int main () {
    string enteredcmd;
    vector<Command*> history;
 
    while(1) {
        getline(cin, enteredcmd);
        // autocomplete for files

        // check for ctrl c, ctrl z, ctrl r

        // implement utilities like cd, exit, help, etc

        // implement multiwatch

        Command* cmd = new Command(enteredcmd);

        if(cmd->isPipe) {
            // Implement Piped Execution
        }
        else if(cmd->isBackground) {
            // Implement Background Execution
        }
        else {
            // Implement Normal Execution with IN an OUT fds provided
        }

        history.push_back(cmd);
    }

    return 0;
}
