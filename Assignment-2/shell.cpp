/*
 * Authors - Debanjan Saha + Pritkumar Godhani
 * Roll - 19CS30014 + 19CS10048
 * Description - Implementing our own shell
 * Operating System Lab
 * © DS&PG 2021 IIT KGP
*/

#include <bits/stdc++.h>
using namespace std;

#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>

struct termios original;

#define BACKSPACE 127
#define MAXHISTSIZE 1000

vector<pid_t> waitingFor;
static sigjmp_buf backtoprompt;
static volatile sig_atomic_t jumpaction = 0;
pid_t parent;

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

void disableRawMode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

void enableRawMode() {
	tcgetattr(STDIN_FILENO, &original);
	// atexit(disableRawMode);
	struct termios raw = original;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    // raw.c_cc[VSUSP] = _POSIX_VDISABLE;
    raw.c_cc[VREPRINT] = _POSIX_VDISABLE;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

class Command {
public:
    string enteredCmd;
    bool isPipe;
    bool isBackground;
    bool isComposite;
    int inFile;
    int outFile;
    string infilename;
    string outfilename;
    vector<Command*> pipeCmds;
    vector<Command*> bgCmds;
    vector<char*> tokens;
    Command(string _enteredCmd) {
        enteredCmd = string(_enteredCmd);
        isComposite = false;
        isBackground = false;
        isPipe = false;
        inFile = STDIN_FILENO;
        outFile = STDOUT_FILENO;
        infilename = "";
        outfilename = "";
    }
};

void readLine(string& line) {
	// clear any buffer present in line from earlier operations
	line.clear();
	char ch;
	while (true) {
		ch = getchar();
		if (ch == BACKSPACE) {
			// Backspace
			if (line.empty()) {
				continue;
			}
			cout << "\b \b"; //Cursor moves 1 position backwards
			if (!line.empty()) {
				line.pop_back();
			}
		} else if (ch == '\t') {
			// Autocomplete
		}
		else  if (ch == '\n') {
			// Newline
			cout << "\n";
			return;
		} else {
			// Anything else
			line += ch;
			cout << ch;
		}
	}
}

int tokenizeCommand(string& command, Command* cmd){
	// cout<<"Received : "<<command<<endl;
	int len = command.length();
	string token = "";

	int head = -1, tail = 0;
	bool foundQuote=false;
	char activeQuote = '\0';
	char singleQuote='\'', doubleQuote='\"';
    vector<string> args;
	// 2 pointer algorithm to tokenize a command
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
            args.push_back(token);
			// char *token_c_str = new char[token.length()+1];
			// strcpy(token_c_str, const_cast<char *>(token.c_str()));
			// cmd->tokens.push_back(token_c_str);
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
		cmd->tokens.clear();
		exit(EXIT_FAILURE);
	}

    vector<string> args_cmd;
    for(int i = 0; i < args.size(); i++) {
        if(args[i] == ">")
        {
            // cout<<"o redirect"<<endl;
            if(i == args.size()-1) {
                cerr<<"ERROR:: argument absent for i/o redirection token."<<endl;
                return -1;
            }
            cmd->outfilename = args[i+1];
            i++;
        }
        else if(args[i] == "<")
        {
            // cout<<"i redirect"<<endl;
            if(i == args.size()-1) {
                cerr<<"ERROR:: argument absent for i/o redirection token."<<endl;
                return -1;
            }
            cmd->infilename = args[i+1];
            i++;
        }  
        else {
            // char *token_c_str = new char[args[i].length()+1];
			// strcpy(token_c_str, const_cast<char *>(args[i].c_str()));
			// cmd->tokens.push_back(token_c_str);
            args_cmd.push_back(args[i]);
        } 
    }

    for(string s : args_cmd) {
        char* token_c_str = new char[s.length()+1];
        strcpy(token_c_str, const_cast<char *>(s.c_str()));
        cmd->tokens.push_back(token_c_str);
        // cout<<"after io redirection : "<<s<<endl;
    }

	cmd->tokens.push_back(NULL);
    return 0;
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
			// Reached end of string
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
        if( tokenizeCommand(cmd->enteredCmd, cmd) < 0) {
            return -1;
        }
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

void sigintHandler(int signo) {
    // if(waitingFor.size() > 1) {
    //     pid_t pid = waitingFor[waitingFor.size()-1];
    //     if(kill(pid, SIGINT) < 0) {
    //         cerr<<"ERROR:: kill() to pid["<<pid<<"] failed [SIGINT]"<<endl;
    //     }
    //     waitingFor.pop_back();
    // }
    if(!jumpaction) return;
    siglongjmp(backtoprompt, 42);
}

void sigtstpHandler(int signo) {
    // signal(SIGINT, SIG_IGN);
    // signal(SIGTSTP, SIG_IGN);
    // kill(getpid(),SIGTSTP);
    pause();
}

int executeSimpleCommand(Command* cmd, int inFD, int outFD) {
    cout<<"Simple CMD : "<<cmd->enteredCmd<<endl;
    pid_t pid;
    if(cmd->isBackground) {
        // Push to background, spawn a new process
        // cout<<"background"<<endl;
        if((pid = fork()) == 0) {
            signal(SIGINT, SIG_IGN);
            signal(SIGTSTP, SIG_IGN);
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
            // pid_t child = getpid();
            setpgid(getpid(), getpid());
            // tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_IGN);
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
            waitingFor.push_back(pid);
            setpgid(pid, pid);
            // wait(NULL);
            int status;
            pid_t x = waitpid(pid, &status, WUNTRACED);
            cout<<"Wait status : "<<status<<endl;
            // cout<<"Waiting stopped.."<<endl;
            if(!WIFEXITED(status) && WIFSTOPPED(status)) {
                cout<<"stopped by : "<<WSTOPSIG(status)<<endl;
                cout<<"Resuming child["<<pid<<"] now..."<<x<<endl;
                try  {
                    kill(pid, SIGCONT);
                } catch (...) {
                    cerr<<"ERROR:: signal transmission SIGCONT failed. "<<endl;
                }
            }
            return 0;
        }
    }
}


int executeCommand(Command* cmd) {
    int numArgs = cmd->tokens.size()-1;

    if(cmd->infilename.length() > 0 && ((cmd->inFile = open(cmd->infilename.c_str(), O_RDONLY)) < 0)) {
        cerr<<"WARNING:: i/o redirection file cannot be accessed... using stdi/o."<<endl;
        cmd->inFile = STDIN_FILENO;
    }

    if(cmd->outfilename.length() > 0 && ((cmd->outFile = open(cmd->outfilename.c_str(), O_WRONLY|O_CREAT, 0666)) < 0)){
        cerr<<"WARNING:: i/o redirection file cannot be accessed... using stdi/o."<<endl;
        cmd->outFile = STDIN_FILENO;
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
                status = executeSimpleCommand(cmd->pipeCmds[i], inFD, pipeFD[1]);
                close(pipeFD[1]);
                inFD = pipeFD[0];
            }
        }
        // put not error later
        if(!pipeErr) {
            //cout<<"And finish."<<endl;
            status = executeSimpleCommand(cmd->pipeCmds[i], inFD, cmd->pipeCmds[i]->outFile);
        }
        return status;
    } 
    else {
        return executeSimpleCommand(cmd, cmd->inFile, cmd->outFile);
    }

}

int fetchHistory(deque<string>& history) {
    string history_file = "./mybash_history";
    ifstream file_in(history_file);
    string line;
    while(file_in) {
        getline(file_in, line);
        if(trim(line).length() > 0)
            history.push_back(trim(line));
    }
    file_in.close();
    return 0;
}

int saveHistory(deque<string>& history) {
    string history_file = "./mybash_history";
    ofstream file_out(history_file);
    for(string x : history) {
        if(x.length() > 0)
            file_out<<x<<endl;   
    }
    file_out.close();
    return 0;
}

string searchHistory(deque<string>& history) {
    return string("");
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

    // waitingFor.push_back(-1);

    string enteredcmd;
    deque<string> history;

    fetchHistory(history);
    parent = getpid();
    signal(SIGINT, sigintHandler);
    signal(SIGTSTP, SIG_IGN);
    setpgid(parent, parent);
    tcsetpgrp(STDIN_FILENO, parent);
    // testParser();
    int i = 0;
    // vector<string> cmdi = {"ls &", "wc|sed &", "ls | wc", "ls"};
    while(1) {
        signal(SIGTSTP, SIG_IGN);
        cout<<">>> ";
        readLine(enteredcmd);
        if (enteredcmd.empty()) {
			// cout << "You have not entered anything!\n";
			continue;
		}
        
        if(history.empty() || (!history.empty() && enteredcmd.compare(history[history.size()-1]) != 0)) {
            if(history.size() >= MAXHISTSIZE) {
                history.pop_front();
            }
            history.push_back(enteredcmd);
        }
        if (enteredcmd.compare("exit") == 0) {
			cout << "Exiting...\n";
			break;
		}
        if (enteredcmd.compare("history") == 0) {
            cout << "Showing history..." << endl;
            for(string x: history) {
                cout<<x<<endl;
            }
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

        if(sigsetjmp(backtoprompt, 1) == 42) {
            cout<<"^C"<<endl;
        }
        jumpaction = 1;
        
    }

    saveHistory(history);

	disableRawMode();

    return 0;
}
