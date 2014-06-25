#include "cmdline_parser.h"

#include <stdlib.h>
#include <string.h>
#include <iostream>

// default constructor:
CmdlineParser::CmdlineParser() : log_file_path_("") {
}

CmdlineParser::~CmdlineParser() {
}

void CmdlineParser::ParseCmdLine(int argc, char** argv) {
	exe_name_ = argv[0];

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") || strcmp(argv[i], "--help")) {
			ShowHelpInfo();
			// quit:
			exit(0);
			
		} else if (strncmp(argv[i], "--log-path=", strlen("--log-path="))) {
			log_file_path_ = argv[i] + strlen("--log-path=");
		} 

	}

}

void CmdlineParser::ShowHelpInfo() {
	std::cout<<std::endl<<"Usage: very similar to GTest"<<std::endl;
}
