#include "coqcic/from_sexpr.h"
#include "coqcic/parse_sexpr.h"

#include <iostream>

void show_error_context(
	const std::string& data,
	std::size_t location
) {
	std::size_t current_line_start = 0;
	for (;;) {
		std::size_t end = data.find('\n', current_line_start);
		if (end == std::string::npos) {
			end = data.size();
		} else {
			end = end + 1;
		}
		if (location >= current_line_start && location < end) {
			std::cerr << data.substr(current_line_start, end - current_line_start);
			std::cerr << std::string(location - current_line_start, ' ') << "^ here\n";
			return;
		}
	}
	std::cerr << "(at end of stream)\n";
}

int main(int argc, char** argv) {
	std::string data;
	while (!std::cin.eof()) {
		std::string tmp;
		std::getline(std::cin, tmp);
		data += std::move(tmp);
		data += '\n';
	}

	auto sexpr = coqcic::parse_sexpr(data);
	if (!sexpr) {
		std::cerr << "Failed to parse sexpr: " << sexpr.error().description << "\n";
		show_error_context(data, sexpr.error().location);
		return 1;
	};
	auto sfb = coqcic::sfb_from_sexpr(sexpr.value());
	if (!sfb) {
		std::cerr << "Failed to parse sfb: " << sfb.error().description << "\n";
		show_error_context(data, sfb.error().context->location());
		return 1;
	}

	std::cout << sfb.value().debug_string() << "\n";
	return 0;
}
