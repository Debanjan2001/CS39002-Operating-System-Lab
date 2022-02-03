#include <bits/stdc++.h>
using namespace std;

#include<iostream>
#include<vector>
#include<termios.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>

struct termios original;

#define BACKSPACE 127

void disableRawMode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

void enableRawMode() {
	tcgetattr(STDIN_FILENO, &original);
	// atexit(disableRawMode);
	struct termios raw = original;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


const string WHITESPACE = " \n\r\t\f\v";
string ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}
string rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}
string trim(const string &s) {
    return rtrim(ltrim(s));
}

class Command {
public:
    string enteredCmd;
    bool isPipe;
    bool isBackground;
    bool isComposite;
    int inFile;
    int outFile;
    vector<Command*> pipeCmds;
    vector<Command*> bgCmds;
    vector<char*> tokens;
    /*Command(string _enteredCmd) {
        enteredCmd = string(_enteredCmd);
        bool readSuccess = parseCommand(_enteredCmd);
    }*/
    Command(string _enteredCmd) {
        enteredCmd = string(_enteredCmd);
        isComposite = false;
        isBackground = false;
        isPipe = false;
        inFile = STDIN_FILENO;
        outFile = STDOUT_FILENO;
    }
};

void readLine(string& line) {
	/*
	 * clear any buffer present in line from earlier operations
	 */
	line.clear();
	char ch;
	while (true) {
		ch = getchar();
		if (ch == BACKSPACE) {
			/*
			 * Backspace
			 */
			if (line.empty()) {
				continue;
			}
			cout << "\b \b"; //Cursor moves 1 position backwards
			if (!line.empty()) {
				line.pop_back();
			}
		} else if (ch == '\t') {
			/*
			 * Autocomplete
			 */
		}
		else  if (ch == '\n') {
			/*
			 * Newline
			 */
			cout << "\n";
			return;
		} else {
			/*
			 * Anything else
			 */
			line += ch;
			cout << ch;
		}
	}
}

void tokenizeCommand(string& command, vector<char *>& tokenized){
	cout<<"Received : "<<command<<endl;
	int len = command.length();
	string token = "";

	int head = -1, tail = 0;
	bool foundQuote=false;
	char activeQuote = '\0';
	char singleQuote='\'', doubleQuote='\"';

	/*
	 * 2 pointer algorithm to tokenize a command
	 */
	while(tail < len){
		while(head + 1<len && (foundQuote || command[head+1] != ' ')){
			head += 1;
			token += command[head];
			if(command[head] == singleQuote || command[head] == doubleQuote){
				if(!foundQuote){
					foundQuote = true;
					activeQuote = command[head];
				} else if(activeQuote == command[head]){
					foundQuote = false;
					activeQuote = '\0';
				}
			} 
		}

		if(head >= tail){
			char *token_c_str = new char[token.length()+1];
			strcpy(token_c_str, const_cast<char *>(token.c_str()));
			tokenized.push_back(token_c_str);
			// cout<<const_cast<const char *>(str)<<endl;
			token.clear();
			tail = head + 1;
			head = tail - 1;
		}else{
			tail += 1;
			head = tail - 1;
		}
	}

	if(foundQuote != false){
		cout<<"ParseError(): Invalid Command\n";
		tokenized.clear();
		exit(EXIT_FAILURE);
	}

	tokenized.push_back(NULL);
}

int splitPipe(string& line, vector<string>& commandList) {

	string command = "";
	int numPipes = 0;

	int len = line.length();
	for(int i=0;i < len;i++){

		if(line[i] == ' ' && command.empty()){
			continue;
		} else if(line[i] == '|'){
			++numPipes;
			while(!command.empty() && command.back() == ' '){
				command.pop_back();
			}
			commandList.push_back(command);
			command.clear();
		} else {
			command += line[i];
		}

		if(i == len-1){
			/*
			 * Reached end of String
			 */
			if(!command.empty()){
				while(!command.empty() && command.back() == ' '){
					command.pop_back();
				}
				commandList.push_back(command);
			}
			command.clear();
		}
	}

	return numPipes;
}


int splitAmpersand(string str, vector<string>& command, string del = "&") {
    int count = 0;
    size_t pos = 0;
    string token;
    while ((pos = str.find(del)) != string::npos) {
        count ++;
        token = str.substr(0, pos+1);
        command.push_back(trim(token));
        str.erase(0, pos + del.length());
    }
    if(trim(str).length() > 0) {
        count++;
        command.push_back(trim(str));
    }
    return count;
}

/*int splitPipe(string str, vector<string>& command, string del = "|") {
    int count = 0;
    size_t pos = 0;
    string token;
    while ((pos = str.find(del)) != string::npos) {
        count ++;
        token = str.substr(0, pos);
        command.push_back(trim(token));
        str.erase(0, pos + del.length());
    }
    if(trim(str).length() > 0) {
        count++;
        command.push_back(trim(str));
    }
    return count;
}*/

int parseCommand(string enteredcmd, Command* cmd, int pipeNum = -1) {
    // parse command and extract all the information
    trim(enteredcmd);
    int len = enteredcmd.length();
    if(len > 0 && enteredcmd[len-1] == '&') {
        cmd->isBackground = true;
        cmd->enteredCmd.pop_back();
        enteredcmd.pop_back();
    }

    vector<string> pipecomm;
    int pipe_count = splitPipe(enteredcmd, pipecomm);
    if(pipe_count > 0) {
        cmd->isPipe = true;
        for(auto s : pipecomm) {
            cmd->pipeCmds.push_back(new Command(s));
            parseCommand(s, cmd->pipeCmds[cmd->pipeCmds.size()-1]);
            if(cmd->isBackground) {
                cmd->pipeCmds[cmd->pipeCmds.size()-1]->isBackground = true;
            }
        }
    }
    else {
        cmd->isPipe = false;
        tokenizeCommand(cmd->enteredCmd, cmd->tokens);
        // stringstream ss(enteredcmd);
        // string nexttoken;
        // while(getline(ss, nexttoken, ' ')) {
        //     cmd->tokens.push_back(nexttoken);
        // }
    }    
    return 0;
    /*vector<string> bgcomm;
    int bg_count = splitAmpersand(enteredcmd, bgcomm);
    if(bg_count > 1) {
        cmd->isComposite = true;
        cmd->isBackground = false;
        cmd->isPipe = false;
        for(auto s : bgcomm) {
            cmd->bgCmds.push_back(new Command(s));
            parseCommand(s, cmd->bgCmds[cmd->bgCmds.size()-1]);
        }
    }
    else {
        cmd->isComposite = false;
        if(enteredcmd[enteredcmd.length()-1] == '&') cmd->isBackground = true;
        vector<string> pipecomm;
        int pipe_count = splitPipe(enteredcmd, pipecomm);
        if(pipe_count > 1) {
            cmd->isPipe = true;
            for(auto s : pipecomm) {
                cmd->pipeCmds.push_back(new Command(s));
                parseCommand(s, cmd->pipeCmds[cmd->pipeCmds.size()-1]);
            }
        }
        else {
            cmd->isPipe = false;
        }
    }*/
}

int executeSimpleCommand(Command* cmd, int inFD, int outFD) {
    // cout<<"Simple CMD : "<<cmd->enteredCmd<<endl;
    pid_t pid;
    if(cmd->isBackground) {
        // Push to background, spawn a new process
        // cout<<"background"<<endl;

        if((pid = fork()) == 0) {
            if(inFD != STDIN_FILENO) {
                dup2(inFD, STDIN_FILENO);
                close(inFD);
            }
            // cout<<"DUPIN"<<endl;
            if(outFD != STDOUT_FILENO) {
                dup2(outFD, STDOUT_FILENO);
                close(outFD);
            }
            // cout<<"DUPOUT"<<endl;
            // cout<<"Execvp call by child\n";
            int status = execvp(cmd->tokens[0], cmd->tokens.data());
            exit(status);
        }
        else {
            return 0;
        }            
    }
    else {
        // Normal process executions
        // cout<<"normal single command"<<endl;
        if((pid = fork()) == 0) {
            if(inFD != STDIN_FILENO) {
                dup2(inFD, STDIN_FILENO);
                close(inFD);
            }
            // cout<<"DUPIN"<<endl;
            if(outFD != STDOUT_FILENO) {
                dup2(outFD, STDOUT_FILENO);
                close(outFD);
            }
            // cout<<"DUPOUT"<<endl;
            // cout<<"Execvp call by child\n";
            int status = execvp(cmd->tokens[0], cmd->tokens.data());
            exit(status);
        }
        else {
            wait(NULL);
            return 0;
        }
    }
}


int executeCommand(Command* cmd) {
    int numArgs = cmd->tokens.size()-1;
    for(int i = 0; i < cmd->tokens.size(); i++) {
        if(cmd->tokens[i] == ">")
        {
            if(i == cmd->tokens.size()-1) {
                cerr<<"ERROR:: argument absent for i/o redirection token."<<endl;
                return -1;
            }
            if((cmd->outFile = open(cmd->tokens[i+1], O_WRONLY | O_CREAT, 0666)) < 0) {
                cerr<<"WARNING:: i/o redirection failed... switching to stdi/o."<<endl;
                cmd->outFile = STDOUT_FILENO;
            }
        }

        if(cmd->tokens[i] == "<")
        {
            if(i == cmd->tokens.size()-1) {
                cerr<<"ERROR:: argument absent for i/o redirection token."<<endl;
                return -1;
            }
            if((cmd->inFile = open(cmd->tokens[i+1], O_RDONLY)) < 0) {
                cerr<<"WARNING:: i/o redirection failed... switching to stdi/o."<<endl;
                cmd->inFile = STDIN_FILENO;
            }
        }   
    }

    if(cmd->isPipe) {
        // Implement piped executions
        int status = 1;
        cout<<"pipe commands"<<endl;
        int pipeFD[2];
        int inFD = cmd->pipeCmds[0]->inFile;
        int i = 0;
        int pipeErr = 0;
        for(i = 0; i < cmd->pipeCmds.size() - 1; i++ ) {
            if(pipe(pipeFD) == -1) {
                cerr<<"ERROR:: pipe() failed."<<endl;
                pipeErr = 1;
            } else {
                status = executeSimpleCommand(cmd->pipeCmds[i], inFD, pipeFD[1]) && status;
                close(pipeFD[1]);
                inFD = pipeFD[0];
            }
        }
        // put not error later
        if(!pipeErr) {
            //cout<<"And finish."<<endl;
            status = executeSimpleCommand(cmd->pipeCmds[i], inFD, cmd->pipeCmds[i]->outFile) && status;
        }
        return status;
    } 
    else {
        return executeSimpleCommand(cmd, cmd->inFile, cmd->outFile);
    }

}

void testParser() {
    vector<string> cmd = {"ls &", "wc|sed &", "ls | wc", "ls"};
    for(int i = 0; i < 4; i++) {
        Command* command = new Command(cmd[i]);
        parseCommand(cmd[i], command);
        cout<<command->isPipe<<" "<<command->isComposite<<" "<<command->isBackground<<endl;
        for(auto i : command->pipeCmds) {
            cout<<"\t"<<i->isPipe<<" "<<i->isComposite<<" "<<i->isBackground<<endl;
        }
    }
}

int main () {

    enableRawMode();

    string enteredcmd;
    vector<Command*> history;
    // testParser();
    int i = 0;
    // vector<string> cmdi = {"ls &", "wc|sed &", "ls | wc", "ls"};
    while(1) {
        cout<<">>> ";
        readLine(enteredcmd);
        if (enteredcmd.empty()) {
			cout << "You have not entered anything!\n";
			continue;
		}
        // autocomplete for files

        // check for ctrl c, ctrl z, ctrl r

        // implement utilities like cd, exit, help, etc

        // implement multiwatch

        Command* cmd = new Command(enteredcmd);
        if(parseCommand(cmd->enteredCmd, cmd) < 0) {
            continue;
        }

        if(executeCommand(cmd) < 0) {
            continue;
        }

        if (enteredcmd.compare("exit") == 0) {
			cout << "Exiting...\n";
			break;
		}
        i++;
        history.push_back(cmd);
    }

	disableRawMode();

    return 0;
}
