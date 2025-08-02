#ifndef PARSER_H 
#define PARSER_H
#include <vector>
#include <string>

struct ParsedData {
	std::vector<int16_t> welos;
	std::vector<int16_t> belos;
	std::vector<int16_t> timeCtl;
	std::vector<int16_t> increment;
	std::vector<int64_t> gamestarts;
	std::vector<std::string> mvs;
	std::vector<int8_t> result;
	std::vector<std::string> clk;
	std::vector<std::string> eval;
	ParsedData(int chunkSize) {
		welos.resize(chunkSize);
		belos.resize(chunkSize);
		timeCtl.resize(chunkSize);
		increment.resize(chunkSize);
		gamestarts.resize(chunkSize);
		mvs.resize(chunkSize);
		result.resize(chunkSize);
		clk.resize(chunkSize);
		eval.resize(chunkSize);
	}
};
#endif
