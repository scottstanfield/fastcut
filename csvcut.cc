/*
    Fastcut: a fast, C++ version of csvkit's csvcut tool
	Copyright © 2016 Chris Idzerda

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <fstream>
#include <iostream>
#include <istream>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>

using vector= std::vector<char const*>;

static char const* prog;

static int usage() {
	std::cerr << "Fastcut v1.0" << std::endl << std::endl;
	std::cerr << "Copyright © 2016 Chris Idzerda" << std::endl;
	std::cerr << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>." << std::endl;
	std::cerr << "This is free software: you are free to change and redistribute it." << std::endl ;
	std::cerr << "There is NO WARRANTY, to the extent permitted by law." << std::endl << std::endl;

	std::cerr << "Print selected columns to standard output." << std::endl;
	std::cerr << std::endl;
	std::cerr << "usage: " << prog << " [-h] [-s] -(c|K) columns [input.csv] [...]" << std::endl << std::endl;
	std::cerr << "\t-s\tskip the header (i.e., the first line)" << std::endl;
	std::cerr << "\t-c\tcomma-separated list of 1-based column ranges to print" << std::endl;
	std::cerr << "\t-K\tcomma-separated list of 0-based column ranges to print" << std::endl << std::endl;
	std::cerr << "Options affect only those files that appear after them.  Specifying options at" << std::endl;
	std::cerr << "the end assumes standard input is the last file." << std::endl;
	return 2;
}

static vector as_parts(std::string& line) {
	bool is_in_quote= false;
	vector rv(1, &line[0]);
	for(auto& ch: line) {
		if(is_in_quote) {
			if(ch == '"') {
				is_in_quote= false;
			}
		} else if(ch == ',') {
			ch= '\0';
			rv.push_back(&ch + 1);
		} else if(ch == '"') {
			is_in_quote= true;
		}
	}

	return rv;
}

static int get_index(char const* begin, char const* end, vector const& parts, bool is_one_based) {
	int index;
	if(std::all_of(begin, end, [](char ch) { return std::isdigit(ch); })) {
		// All characters are digits; parse as a number.
		index= std::atoi(begin);
		if(is_one_based) {
			if(index < 1) {
				std::cerr << prog << ": invalid column specification " << index << std::endl;
				exit(2);
			}
			--index;
		}
		if(index >= static_cast<int>(parts.size())) {
			if(is_one_based) {
				++index;
			}
			std::cerr << prog << ": invalid column specification " << index << std::endl;
			exit(2);
		}
	} else {
		// Some characters are not digits; parse as a column name.
		auto s= std::string(begin, end);
		auto it= std::find(parts.begin(), parts.end(), s);
		if(it == parts.end()) {
			std::cerr << prog << ": cannot find '" << s << "' in header" << std::endl;
			exit(1);
		}
		index= it - parts.begin();
	}
	return index;
}

static void parse_and_cut(char* specification, std::istream& sin, bool is_one_based, bool wants_header) {
	// Check for problems.
	char const* range_token= std::strtok(specification, ",");
	if(!range_token) {
		std::cerr << prog << ": no column specification provided" << std::endl << std::endl;
		exit(usage());
	}

	// Read the first line since I might need it for the specification.
	std::string s;
	if(!std::getline(sin, s)) {
		return; // No data; don't bother.
	}
	vector first_parts= as_parts(s);

	// Parse each range.
	std::vector<int> indices;
	do {
		// Check if this is a range.
		char const* end_of_range= std::strrchr(range_token, '-');
		if(end_of_range) {
			// Get the first index of the range.
			int first_index;
			if(range_token == end_of_range) {
				// There is no first index; it's open on the left.
				first_index= 0;
			} else {
				first_index= get_index(range_token, end_of_range, first_parts, is_one_based);
			}

			// Get the last index of the range.
			range_token= end_of_range + 1;
			int last_index;
			if(*range_token) {
				end_of_range= range_token + strlen(range_token);
				last_index= get_index(range_token, end_of_range, first_parts, is_one_based);
			} else {
				// There is no last index; it's open on the right.
				last_index= first_parts.size() - 1;
			}

			// Add the range to the collection of indices.
			if(last_index < first_index) {
				for(int i= first_index; i >= last_index; --i) {
					indices.push_back(i);
				}
			} else {
				for(int i= first_index; i <= last_index; ++i) {
					indices.push_back(i);
				}
			}
		} else {
			// It's not a range.
			end_of_range= range_token + strlen(range_token);
			int index= get_index(range_token, end_of_range, first_parts, is_one_based);
			indices.push_back(index);
		}
	} while(range_token= std::strtok(nullptr, ","), range_token);

	// Print the first line, if requested.
	if(wants_header) {
		for(int i= 0, n= indices.size(); i < n; ++i) {
			std::cout << first_parts[indices[i]];
			if(i < n - 1) {
				std::cout << ',';
			} else {
				std::cout << std::endl;
			}
		}
	}

	// Read and print the rest of the lines.
	while(std::getline(sin, s)) {
		vector first_parts= as_parts(s);
		for(int i= 0, n= indices.size(); i < n; ++i) {
			if(indices[i] < static_cast<int>(first_parts.size())) {
				std::cout << first_parts[indices[i]];
			}
			if(i < n - 1) {
				std::cout << ',';
			} else {
				std::cout << std::endl;
			}
		}
	}
}

int main(int argc, char* argv[]) {
	// Extract the name of the program for nicer usage and error reports.
	prog= strrchr(argv[0], '/');
	if(prog == nullptr) {
		prog= argv[0];
	} else {
		++prog;
	}

	// Parse options and process files.
	char* specification= nullptr;
	char const* file_name= nullptr;
	bool is_one_based= false, wants_header= true;
	int ch;
	while(ch= getopt(argc, argv, "+sc:K:h"), ch != -1 || optind < argc) {
		switch(ch) {
		case '?':
		case 'h':
			return usage();
		case 's':
			wants_header= false;
			break;
		case 'c':
			specification= optarg;
			file_name= nullptr;
			is_one_based= true;
			break;
		case 'K':
			specification= optarg;
			file_name= nullptr;
			is_one_based= false;
			break;
		case -1:
			file_name= argv[optind];
			std::ifstream fin(file_name);
			if(!fin) {
				std::cerr << prog << ": cannot open '" << file_name << "' for reading" << std::endl;
				exit(1);
			}
			parse_and_cut(specification, fin, is_one_based, wants_header);
			optind += 1;
			break;
		}
	}

	if(file_name == nullptr) {
		// Process standard input.
		parse_and_cut(specification, std::cin, is_one_based, wants_header);
	}

	return 0;
}
