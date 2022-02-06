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
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>

struct termios original;

#define BACKSPACE 127
#define MAXHISTSIZE 1000
#define MAX_BUFFER_SIZE 1024

vector<pid_t> waitingFor;
static sigjmp_buf backtoprompt;
static volatile sig_atomic_t jumpaction = 0;
pid_t parent;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
deque<string> history;

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
        else if (iscntrl(ch) && ch == 18) {
            // CTRL R pressed
            // wait for autocomplete
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
			token.clear();
			tail = head + 1;
			head = tail - 1;
		}else{
			tail += 1;
			head = tail - 1;
		}
	}

	if(foundQuote != false){
		cerr<<"ERROR:: Invalid Command\n";
		cmd->tokens.clear();
        return -1;
		// exit(EXIT_FAILURE);
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
}

void sigintHandler(int signo) {
    if(!jumpaction) return;
    siglongjmp(backtoprompt, 42);
}

void sigtstpHandler(int signo) {
    pause();
}

int executeSimpleCommand(Command* cmd, int inFD, int outFD, int mw_mode = 0) {
    // cout<<"Simple CMD : "<<cmd->enteredCmd<<endl;
    pid_t pid, pgid = 0;
    pid = fork();
    if(pid == 0) {
        if(!mw_mode) {
            pid = getpid();
            if(pgid == 0) pgid = pid;
            setpgid(pid, pgid);
        }
        if(!cmd->isBackground)
            tcsetpgrp(shell_terminal, pgid);
        signal (SIGINT, SIG_DFL);
        signal (SIGQUIT, SIG_DFL);
        signal (SIGTSTP, SIG_DFL);
        signal (SIGTTIN, SIG_DFL);
        signal (SIGTTOU, SIG_DFL);
        signal (SIGCHLD, SIG_DFL);
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
        int status = execvp(cmd->tokens[0], cmd->tokens.data());
        exit(status);
    }
    else {
        if(!mw_mode) {
            if(!pgid) pgid = pid;
            setpgid(pid, pgid);
        }
    }

    if(!cmd->isBackground) {
        tcsetpgrp(shell_terminal, pgid);
        int status;
        pid_t x = waitpid(pid, &status, WUNTRACED);
        // cout<<"Wait Status : "<<status<<endl;
        // cout<<"Waiting stopped.."<<endl;
        if(!WIFEXITED(status) && WIFSTOPPED(status)) {
            // cout<<"Stopped by : "<<WSTOPSIG(status)<<endl;
            // cout<<"Resuming child["<<pid<<"] now..."<<x<<endl;
            if(kill(-pid, SIGCONT) < 0) {
                cerr<<"ERROR:: transmission of SIGCONT failed."<<endl;
            }
        }
        tcsetpgrp(shell_terminal, shell_pgid);
        struct termios job_tmodes;
        tcgetattr(shell_terminal, &job_tmodes);
        tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);    
    }
    else{
        // cout<<"Background execution"<<endl;
    }
    return 0;
}


int executeCommand(Command* cmd, int mw_mode = 0) {
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
        // cout<<"pipe commands"<<endl;
        int pipeFD[2];
        int inFD = cmd->pipeCmds[0]->inFile;
        int i = 0;
        int pipeErr = 0;
        for(i = 0; i < cmd->pipeCmds.size() - 1; i++ ) {
            if(pipe(pipeFD) == -1) {
                cerr<<"ERROR:: pipe() failed."<<endl;
                pipeErr = 1;
            } else {
                status = executeSimpleCommand(cmd->pipeCmds[i], inFD, pipeFD[1], mw_mode);
                close(pipeFD[1]);
                inFD = pipeFD[0];
            }
        }
        // put not error later
        if(!pipeErr) {
            //cout<<"And finish."<<endl;
            status = executeSimpleCommand(cmd->pipeCmds[i], inFD, cmd->pipeCmds[i]->outFile, mw_mode);
        }
        return status;
    } 
    else {
        return executeSimpleCommand(cmd, cmd->inFile, cmd->outFile, mw_mode);
    }

}

int loadHistory(deque<string>& history) {
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

int lcs(string &X, string &Y) {
    int m = X.length(), n = Y.length();
    int L[2][n + 1];
    bool f;
    for (int i = 0; i <= m; i++) {
        f = i & 1;
        for (int j = 0; j <= n; j++) {
            if (i == 0 || j == 0) L[f][j] = 0;
            else if (X[i-1] == Y[j-1]) L[f][j] = L[1 - f][j - 1] + 1;
            else L[f][j] = max(L[1 - f][j], L[f][j - 1]);
        }
    }
    return L[f][n];
}

int searchHistory(deque<string>& history, string& searchstr) {
    set<pair<int,int>> lcsindex;

    for(int i = 0; i < history.size(); i++) {
        lcsindex.insert(make_pair(-lcs(history[i], searchstr), -i));
    }

    pair<int,int> result = *(lcsindex.begin());
    if(result.first == 0) {
        return -1;
    } else {
        return -result.second;
    }
}

int parseCommand(string entered_cmd, vector<Command*>& watchedCmds) {
    string cmd = trim(entered_cmd);
    int closeSqB = cmd.find(']');
    if(closeSqB == string::npos) {
        cerr<<"ERROR:: incorrect multiwatch syntax, missing \']\'"<<endl;
        return -1;
    }
    int nextcloseSqB = cmd.find(']', closeSqB+1);
    if(nextcloseSqB != string::npos) {
        cerr<< "ERROR:: incorrect multiwatch syntax, extra \']\'"<<endl;
        return -1;
    }
    int openSqB = cmd.find('[');
    if(openSqB == string::npos) {
        cerr<<"ERROR:: incorrect multiwatch syntax, missing \'[\'"<<endl;
        return -1;
    }
    int nextopenSqB = cmd.find('[', openSqB+1);
    if(nextopenSqB != string::npos) {
        cerr<< "ERROR:: incorrect multiwatch syntax, extra \'[\'"<<endl;
        return -1;
    }
    if(openSqB >= closeSqB) {
        cerr<< "ERROR:: incorrect multiwatch syntax, \']\' precedes \'[\'"<<endl;
        return -1;
    }

    string cmdliststr = cmd.substr(openSqB+1,closeSqB-openSqB-1);
    // cout<<cmdliststr<<endl;

    vector<string> cmdlist;
    splitAmpersand(cmdliststr, cmdlist, ",");

    for(auto s:cmdlist) {
        if((*(s.begin()) != '\"')&&(*(s.end()-1) != '\"')) {
            cerr<< "ERROR:: incorrect multiwatch syntax"<<endl;
            return -1;
        }
        else {
            // cout<<">"<<s<<"<"<<endl;
            string cmdstr ;
            if(s == *(cmdlist.end()-1))
                cmdstr = s.substr(1,s.length()-2);
            else 
                cmdstr = s.substr(1,s.length()-3);
            // cout<<"cmd:"<<cmdstr<<endl;
            Command* nextcmd = new Command(cmdstr);
            if(parseCommand(nextcmd->enteredCmd, nextcmd) < 0) {
                cerr<<"WARNING:: skipping cmd["<<cmdstr<<"]"<<endl;
                continue;
            } 
            nextcmd->isBackground = true;
            watchedCmds.push_back(nextcmd);
        }
    }
    return 0;
}

int handleMultiwatch(string entered_cmd) {
    // start
    // Parse commands into a vector<Command*>
    // Open as many file descriptors as there are commands
    // Fork the commands
    // Give the write access to the commands
    // Collect all the read descriptors in a separate FD_set
    // Select from FD_set
    // Use some identification for which process was selected
    // keep printing until all child do not stop
    // do the pgid voodoo here to stop on ctrl c
    // end
    // doubt : how will these process take inputs ? or do we assume them to work without inputs
    pid_t surrogate, surrogate_grp = 0;
    surrogate = fork();
    if(surrogate == 0) {
        surrogate = getpid();
        if(!surrogate_grp) surrogate_grp = surrogate;
        setpgid(surrogate, surrogate_grp);
        tcsetpgrp(shell_terminal, surrogate_grp);
        signal(SIGINT, SIG_DFL);
        
        vector<Command*> watchedCmds;
        // cout<<entered_cmd<<endl;
        
        if(parseCommand(entered_cmd, watchedCmds)<0) {
            exit(-1);
        }

        int numWatched = watchedCmds.size();
        // cout<<numWatched<<endl;

        vector<int> writefds;
        vector<int> readfds;
        
        int pipes[numWatched][2];
        for(int i = 0; i < numWatched; i++) {
            if(pipe(pipes[i]) < 0) {
                cerr<<"ERROR:: pipe() failed."<<endl;
                exit(-1);
            }
            int fd = pipes[i][1];
            writefds.push_back(fd);

            int rfd = pipes[i][0];
            readfds.push_back(rfd);
        }
        vector<pid_t> processes;
        for(int i = 0; i < numWatched; i++) {
            pid_t pid;
            pid = fork();
            if(pid == 0) {
                dup2(writefds[i], STDOUT_FILENO);
                close(writefds[i]);

                executeCommand(watchedCmds[i], 1);
                exit(1);
            } else {
                processes.push_back(pid);
            }
        } 

        int nfds, openfds;
        struct pollfd* pollfdset;
        openfds = nfds = numWatched;

        pollfdset = (struct pollfd*)calloc(nfds, sizeof(struct pollfd));
        if(pollfdset == NULL) {
            cerr<<"ERROR:: calloc() failed."<<endl;
        }

        for(int i = 0; i < nfds; i++) {
            pollfdset[i].fd = pipes[i][0];
            pollfdset[i].events = POLLIN;
        }

        while(openfds > 0) {
            int ready = poll(pollfdset, nfds, -1);
            if(ready == -1) {
                cerr<<"ERROR:: bad poll()."<<endl;
                exit(-1);
            }
            
            for(int j = 0; j < nfds; j++) {
                char buffer[MAX_BUFFER_SIZE];
                if (pollfdset[j].revents != 0) {
                    // printf("  fd=%d; events: %s%s%s\n", pollfdset[j].fd, (pollfdset[j].revents & POLLIN)  ? "POLLIN "  : "",(pollfdset[j].revents & POLLHUP) ? "POLLHUP " : "",(pollfdset[j].revents & POLLERR) ? "POLLERR " : "");
                    if (pollfdset[j].revents & POLLIN) {
                        memset(buffer, 0, MAX_BUFFER_SIZE);
                        ssize_t s = read(pollfdset[j].fd, buffer, sizeof(buffer));
                        if (s == -1) {
                            cerr<<"ERROR:: read() failed."<<endl;
                        }
                        cout<<endl;
                        cout<<"\""<<watchedCmds[j]->enteredCmd<<"\", current_time:"<<(unsigned long)time(NULL)<<endl;
                        cout<<"<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-"<<endl;
                        cout<<buffer;
                        cout<<"->->->->->->->->->->->->->->->->->->->"<<endl<<endl;
                        // printf("    read %zd bytes: %.*s\n",s, (int) s, buffer);
                    } else {                /* POLLERR | POLLHUP */
                        // printf("    closing fd %d\n", pollfdset[j].fd);
                        if (close(pollfdset[j].fd) == -1) {
                            cerr<<"ERROR:: close() failed."<<endl;
                        }
                        openfds--;
                    }
                }
            }
        }
        
        return 0;
    }
    else {
        if(!surrogate_grp) surrogate_grp = surrogate; 
        setpgid(surrogate, surrogate_grp);

        tcsetpgrp(shell_terminal, surrogate_grp);
        int status;
        pid_t x = waitpid(surrogate, &status, WUNTRACED);
        // cout<<"Wait Status : "<<status<<endl;
        // cout<<"Waiting stopped.."<<endl;
        
        tcsetpgrp(shell_terminal, shell_pgid);
        struct termios job_tmodes;
        tcgetattr(shell_terminal, &job_tmodes);
        tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
        if(WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 0;
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

    // waitingFor.push_back(-1);
    string enteredcmd;

    loadHistory(history);
    parent = getpid();

    shell_terminal = STDIN_FILENO;
    while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
    kill (- shell_pgid, SIGTTIN);

    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
    signal (SIGCHLD, SIG_IGN);

    shell_pgid = getpid ();
    if (setpgid (shell_pgid, shell_pgid) < 0)
    {
        perror ("Couldn't put the shell in its own process group");
        exit (1);
    }

    tcsetpgrp (shell_terminal, shell_pgid);

    tcgetattr (shell_terminal, &shell_tmodes);

    // testParser();
    // int i = 0;
    // vector<string> cmdi = {"ls &", "wc|sed &", "ls | wc", "ls"};
    
    while(1) {
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
            // cout << "Showing history..." << endl;
            for(string x: history) {
                cout<<x<<endl;
            }
            continue;
        }
        stringstream ss(enteredcmd);
        string cmdName; ss>>cmdName;
        if(cmdName.compare("multiwatch") == 0) {
            // cout<<"Starting multiwatch..."<<endl;
            if(handleMultiwatch(enteredcmd) < 0) {
                cerr<<"Multiwatch failed"<<endl;
            }
            signal (SIGINT, SIG_IGN);
            continue;
        }
        // implement utilities like cd, exit, help, etc

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
