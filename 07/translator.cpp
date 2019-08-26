#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
#include <filesystem>
#include <utility>

using namespace std;
namespace fs = std::filesystem;

vector<string> split(const string& s) {
	auto space = [] (const char& c) { return isspace(c); };
	auto nspace = [] (const char& c) { return !isspace(c); };
	
	vector<string> retval;
	
	for(auto st = find_if(s.begin(), s.end(), nspace),
		ed = find_if(st, s.end(), space);
		st != s.end();
		st = find_if(ed, s.end(), nspace),
		ed = find_if(st, s.end(), space))
			retval.emplace_back(st, ed);
	
	return retval;
}

struct Command {
	string 	name;
	string 	arg1;
	string 	arg2;
	
	Command(string name, string arg1, string arg2) :
		name(name),
		arg1(arg1),
		arg2(arg2) {}
};


class Parser {
private:
	vector<Command> commands;
	size_t pos;
	
	void parse(stringstream& ss);
public:
	Parser(stringstream ss);
	bool has_next();
	const Command& get_next();
};

class Translator {
private:
	vector<string> commands;
	void process(const Command& c);
	
	const map<string, string> segment = {
		{"local", "LCL"},
		{"argument", "ARG"},
		{"this", "THIS"},
		{"that", "THAT"},
		{"temp", "R5"},
		{"pointer", "R3"}
	};
	
	const map<string, bool> indiracces = {
		{"local", true},
		{"argument", true},
		{"this", true},
		{"that", true},
		{"temp", false},
		{"pointer", false}
	};
	
	int bcount;
	string curfile;
	
	void prebin();
	void preunr();
	
	void boolop(const string& cond);
	void mathop(const string& oper);
	
	void pushd();
	void poptod();
	void constd(const string& c);
	
	void count(const string& seg);
	
public:
	Translator(const vector<pair<string, string> >& data);
	string generate();
};

Parser::Parser(stringstream ss) :
	pos(0) {
	parse(ss);
}

void Parser::parse(stringstream& ss) {
	string line;
	while(getline(ss, line)) {
		size_t compos = line.find("//");
		if(compos != string::npos)
			line.erase(line.begin() + compos, line.end());
		
		vector<string> lexs = split(line);
		if(!lexs.size()) continue;
		while(lexs.size() < 3) lexs.emplace_back("");
		commands.emplace_back(lexs[0], lexs[1], lexs[2]);
	}
}

bool Parser::has_next() {
	return pos < commands.size();
}

const Command& Parser::get_next() {
	return commands[pos++];
}

Translator::Translator(const vector<pair<string, string> >& data) {
	commands.emplace_back("@256");
	commands.emplace_back("D=A");
	commands.emplace_back("@SP");
	commands.emplace_back("M=D");
	
	for(const auto& entry: data) {
		bcount = 0;
		curfile = entry.first;
		
		Parser p(stringstream(entry.second));
		while(p.has_next()) 
			process(p.get_next());
	} 
}

void Translator::process(const Command& c) {
	if(c.name == "push") {
		if(c.arg1 == "static") {
			commands.emplace_back("@STATIC" + curfile + c.arg2);
			commands.emplace_back("D=M");
		}
		else {
			constd(c.arg2);
			if(c.arg1 != "constant") {
				count(c.arg1);
				commands.emplace_back("D=M");
			}
		}
		pushd();
	}
	else if(c.name == "pop") {
		if(c.arg1 == "static") {
			commands.emplace_back("@STATIC" + curfile + c.arg2);
		}
		else {
			constd(c.arg2);
			count(c.arg1);
		}
		commands.emplace_back("D=A");
		poptod();
	}
	else if(c.name == "add") {
		prebin();
		mathop("D+M");
	}
	else if(c.name == "sub") {
		prebin();
		mathop("M-D");
	}
	else if(c.name == "and") {
		prebin();
		mathop("D&M");
	}
	else if(c.name == "or") {
		prebin();
		mathop("D|M");
	}
	else if(c.name == "eq") {
		prebin();
		boolop("JEQ");
	}
	else if(c.name == "gt") {
		prebin();
		boolop("JGT");
	}
	else if(c.name == "lt") {
		prebin();
		boolop("JLT");
	}
	else if(c.name == "not") {
		preunr();
		mathop("!M");
	}
	else if(c.name == "neg") {
		preunr();
		mathop("-M");
	}
	else abort();
}

void Translator::mathop(const string& oper) {
	commands.emplace_back("M=" + oper);
}

void Translator::boolop(const string& cond) {
	commands.emplace_back("D=M-D");
	commands.emplace_back("@BOOLEAN" + curfile + 
							to_string(bcount) + "TRUE");
	commands.emplace_back("D;" + cond);
	commands.emplace_back("@SP");
	commands.emplace_back("A=M-1");
	commands.emplace_back("M=0");
	commands.emplace_back("@BOOLEAN" + curfile + 
							to_string(bcount) + "FALSE");
	commands.emplace_back("0;JMP");
	commands.emplace_back("(BOOLEAN" + curfile + 
							to_string(bcount) + "TRUE)");
	commands.emplace_back("@SP");
	commands.emplace_back("A=M-1");
	commands.emplace_back("M=-1");
	commands.emplace_back("(BOOLEAN" + curfile + 
							to_string(bcount) + "FALSE)");
	bcount++;
}

void Translator::preunr() {
	commands.emplace_back("@SP");
	commands.emplace_back("A=M-1");
}

void Translator::prebin() {
	commands.emplace_back("@SP");
	commands.emplace_back("M=M-1");
	commands.emplace_back("A=M");
	commands.emplace_back("D=M");
	commands.emplace_back("@SP");
	commands.emplace_back("A=M");
	commands.emplace_back("A=A-1");
}

void Translator::pushd() {
	commands.emplace_back("@SP");
	commands.emplace_back("A=M");
	commands.emplace_back("M=D");
	commands.emplace_back("@SP");
	commands.emplace_back("M=M+1");
}

void Translator::constd(const string& c) {
	commands.emplace_back("@" + c);
	commands.emplace_back("D=A");
}

void Translator::count(const string& seg) {
	commands.emplace_back("@" + segment.at(seg));
	if(indiracces.at(seg))
		commands.emplace_back("A=M");
	commands.emplace_back("A=A+D");
}

void Translator::poptod() {
	commands.emplace_back("@R13");
	commands.emplace_back("M=D");
	commands.emplace_back("@SP");
	commands.emplace_back("M=M-1");
	commands.emplace_back("A=M");
	commands.emplace_back("D=M");
	commands.emplace_back("@R13");
	commands.emplace_back("A=M");
	commands.emplace_back("M=D");
}

string Translator::generate() {
	string retval;
	for(const string& s: commands)
		retval += s + "\n";
	return retval;
}

int main(int argc, char* argv[]) {
	if(argc != 2) {
		cout << "Usage: " << argv[0] << " FILE|DIR" <<endl;
		exit(1);
	}
	
	fs::path arg(argv[1]);

	if(!fs::exists(arg)) {
		cout << "Path not found" << endl;
		exit(1);
	}
	
	vector<fs::path> files;
	if(fs::is_regular_file(arg)) {
		if(arg.extension() != ".vm") {
			cout << "Not a .vm file" << endl;
			exit(1);
		}
		files.push_back(arg);
	}
	else if(fs::is_directory(arg)) {
		for(auto& entry: fs::directory_iterator(arg)) {
			if( fs::is_regular_file(entry.path()) &&
				entry.path().extension() == ".vm")
				files.push_back(entry.path());
		}
	}
	else {
		cout << "Path is not dir or file" << endl;
		exit(1);
	}
	
	if(files.size() == 0) {
		cout << "No .vm files found" << endl;
		exit(1);
	}
	
	vector<pair<string, string> > data;
	for(auto p: files) {
		ifstream ifs(p);
		string content( (istreambuf_iterator<char>(ifs)), 
						(istreambuf_iterator<char>()));
		ifs.close();
		
		p.replace_extension("");
		
		data.emplace_back(p.filename(), content);
	}
	
	Translator t(data);
	
	if(fs::is_directory(arg))
		arg /= arg.filename();
	arg.replace_extension(".asm");
	
	ofstream ofs(arg);
	ofs << t.generate();
	ofs.close();
}
