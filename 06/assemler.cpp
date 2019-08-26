#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <bitset>
#include <sstream>

using namespace std;

bool is_number(const string& s) {
    return !s.empty() && find_if(s.begin(), 
        s.end(), [] (char c) { return !isdigit(c); }) == s.end();
}

size_t to_number(const string& s) {
	stringstream ss;
	size_t retval;
	
	ss << s;
	ss >> retval;
	
	return retval;
}

string to_binary(const size_t& val) {
	return bitset<15>(val).to_string();
} 

struct command {
	enum {
		L_COMMAND,
		A_COMMAND,
		C_COMMAND,
	} type;
	vector<string> lexs;
};

class Parser 
{
private:
    vector<command> commands;
    size_t pos;
    
    inline vector<string> read(const string& fname);
    inline vector<command> parse(const vector<string>& lines);
public:
	Parser(const string& fname);
	
	const command& get_next();
	bool has_next();
	void reset();
};

class Assembler {
private:
	const map<string, string> compmap =
	{
		{"0", 	"0101010"},
		{"1", 	"0111111"},
		{"-1", 	"0111010"},
		{"D", 	"0001100"},
		{"A", 	"0110000"},
		{"!D", 	"0001101"},
		{"!A", 	"0110001"},
		{"-D", 	"0001111"},
		{"-A", 	"0110011"},
		{"D+1", "0011111"},
		{"A+1", "0110111"},
		{"D-1", "0001110"},
		{"A-1", "0110010"},
		{"D+A", "0000010"},
		{"D-A", "0010011"},
		{"A-D", "0000111"},
		{"D&A", "0000000"},
		{"D|A", "0010101"},
		{"M", 	"1110000"},
		{"!M", 	"1110001"},
		{"-M", 	"1110011"},
		{"M+1", "1110111"},
		{"M-1", "1110010"},
		{"D+M", "1000010"},
		{"D-M", "1010011"},
		{"M-D", "1000111"},
		{"D&M", "1000000"},
		{"D|M", "1010101"}
	};
	const map<string, string> jmpmap = 
	{
		{"", 	"000"},
		{"JGT", "001"},
		{"JEQ", "010"},
		{"JGE", "011"},
		{"JLT", "100"},
		{"JNE", "101"},
		{"JLE", "110"},
		{"JMP", "111"}
	};
	const map<string, string> destmap = 
	{
		{"", 	"000"},
		{"M", 	"001"},
		{"D", 	"010"},
		{"MD", 	"011"},
		{"A", 	"100"},
		{"AM", 	"101"},
		{"AD", 	"110"},
		{"AMD", "111"}
	};
	
	map<string, size_t> table = 
	{
		{"SP",  	0},
		{"LCL", 	1},
		{"ARG", 	2},
		{"THIS", 	3},
		{"THAT", 	4},
		{"R0", 		0},
		{"R1", 		1},
		{"R2", 		2},
		{"R3", 		3},
		{"R4", 		4},
		{"R5", 		5},
		{"R6", 		6},
		{"R7", 		7},
		{"R8", 		8},
		{"R9", 		9},
		{"R10", 	10},
		{"R11", 	11},
		{"R12", 	12},
		{"R13", 	13},
		{"R14", 	14},
		{"R15", 	15},
		{"SCREEN", 	16384},
		{"KBD", 	24576}
	};
	
	void labels_gen();
	void commands_gen();
	
	Parser parser;
	vector<string> commands;
	bool generated;
public:
	Assembler(const string& fname);
	void generate(const string& fname);
};

Assembler::Assembler(const string& fname) :
	parser(fname),
	generated(false) {}

void Assembler::generate(const string& fname) {
	if(!generated) {
		labels_gen();
		commands_gen();
		generated = true;
	}
	
	ofstream file(fname);
	
	assert(file.is_open());
	
	for(const auto& c: commands) 
		file << c << endl;
		
	file.close();
}

void Assembler::labels_gen() {
	size_t line = 0;
	while(parser.has_next()) {
		const command& c = parser.get_next();
		if(c.type != command::L_COMMAND)
			line++;
		else 
			table[c.lexs[0]] = line;
	}
	
	parser.reset();
}

void Assembler::commands_gen() {
	size_t ipos = 16;
	while(parser.has_next()) {
		const command& c = parser.get_next();
		if(c.type == command::L_COMMAND)
			continue;
		
		string com;
		if(c.type == command::A_COMMAND) {
			com += "0";
			if(is_number(c.lexs[0]))
				com += to_binary(to_number(c.lexs[0]));
			else {
				if(table.count(c.lexs[0]) == 0) 
					table[c.lexs[0]] = ipos++;
				com += to_binary(table.at(c.lexs[0]));
			}
		}
		else com = "111" + compmap.at(c.lexs[1])
						 + destmap.at(c.lexs[0]) 
						 + jmpmap.at(c.lexs[2]);
				
		commands.push_back(com);
	}
	
	parser.reset();
}

vector<string> Parser::read(const string& fname) {
	vector<string> retval;
	
	ifstream file(fname);
	
	assert(file.is_open());
	
	string line;
	while(!file.eof()) {
		getline(file, line);
		
		line.erase(
			remove_if(line.begin(), line.end(), 
				[] (const unsigned char c) {
					return isspace(c);
				}),
			line.end()
		);
		
		size_t compos = line.find(string("//"));
		if(compos != string::npos)
			line.erase(
				line.begin() + compos,
				line.end()
			);
			
		if(line.size() > 0) 
			retval.push_back(line);
	}
	
	file.close();
	
	return retval;
}

vector<command> Parser::parse(const vector<string>& lines) {
	vector<command> retval;
	for(const string& com: lines) {
		command tmp;
		if(com[0] == '@') {
			tmp.type = command::A_COMMAND;
			
			tmp.lexs.emplace_back(com.begin() + 1, com.end());
		}
		else if(
			(com[0] == '(') && 
			(com[com.size() - 1] == ')')
			) {
			tmp.type = command::L_COMMAND;
			
			tmp.lexs.emplace_back(com.begin() + 1, com.end() - 1);
		}
		else {
			tmp.type = command::C_COMMAND;
			
			size_t eqpos = com.find('=');		
			size_t dtpos = com.find(';');
			
			bool eqnpos = eqpos == string::npos;
			bool dtnpos = dtpos == string::npos;
			
			if(eqnpos)
				tmp.lexs.emplace_back("");
			else
				tmp.lexs.emplace_back(com.begin(), com.begin() + eqpos);
				
			size_t st = eqnpos ? 0 : eqpos + 1;
			size_t ed = dtnpos ? com.size() : dtpos;
			tmp.lexs.emplace_back(com.begin() + st, com.begin() + ed);
			
			if(dtnpos)
				tmp.lexs.emplace_back("");
			else
				tmp.lexs.emplace_back(com.begin() + dtpos + 1, com.end());
		}
		retval.push_back(tmp);
	}
	return retval;
}

void Parser::reset() {
	pos = 0;
}

Parser::Parser(const string& fname) :
	pos(0) {
	commands = parse(read(fname));
}

const command& Parser::get_next() {
	return commands[pos++];
}

bool Parser::has_next() {
	return pos < commands.size();
}

int main(int argc, char* argv[]) {
	if(argc == 2) {
		string fname(argv[1]);
		size_t it = fname.find(".asm");
		if(it == string::npos) {
			cout<< "Invalid extension" <<endl;
			return 1;
		}
		Assembler a(fname);
		
		fname.erase(it, fname.size() - it);
		fname += ".hack";
		cout<<fname<<endl;
		
		a.generate(fname);
	}
}

