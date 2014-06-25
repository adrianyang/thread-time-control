#ifndef __CMDLINE_PARSER_H__
#define __CMDLINE_PARSER_H__

#include <string>

// delcaration:
class ThreadTestContainer;

class CmdlineParser {
 public:
	friend class ThreadTestContainer;
 	
	CmdlineParser();
	~CmdlineParser();

 private:
	// We only need ThreadTestContainer to gain access.
	void ParseCmdLine(int argc, char** argv);
	
	void ShowHelpInfo();

	// log path:
	std::string log_file_path_;
	// log level, ...

	
	// exe name: (used by ELF reading)
	std::string exe_name_;
 
};

#endif
