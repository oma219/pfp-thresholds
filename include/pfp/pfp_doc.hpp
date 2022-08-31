/*
 * File: pfp_doc.hpp
 * Description: Header file for pfp_doc data-structure, contains the
 *              construction of the document profile data-structure
 *              and other needed structs for pfp_doc.cpp
 * Date: August 30th, 2022
 */

#ifndef PFP_DOC_H
#define PFP_DOC_H

#include <string>
#include <iostream>
#include <filesystem>
#include <vector>

/* Useful MACROs */
#define FATAL_ERROR(...) do {std::fprintf(stderr, "\nError: "); std::fprintf(stderr, __VA_ARGS__);\
                              std::fprintf(stderr, "\n\n"); std::exit(1);} while(0)
#define ASSERT(condition, msg) do {if (!condition){std::fprintf(stderr, "Assertion Failed: %s\n", msg); \
                                                   std::exit(1);}} while(0)

/* Function declations */
int pfpdoc_usage();
int build_main(int argc, char** argv);
int run_main(int argc, char** argv);
int pfpdoc_build_usage();
int is_file(std::string path);
int is_dir(std::string path);
std::vector<std::string> split(std::string input, char delim);
bool is_integer(const std::string& str);
bool endsWith(const std::string& str, const std::string& suffix);

struct PFPDocBuildOptions {
    public:
        std::string input_list = "";
        std::string output_prefix = "";
        std::string output_ref = "";
        size_t pfp_w = 10;

        void validate() {
            /* checks the arguments and make sure they are valid */
            if (input_list.length() && !is_file(input_list)) // provided a file-list
                FATAL_ERROR("The provided file-list is not valid.");
            else if (input_list.length() == 0)
                FATAL_ERROR("Need to provide a file-list for processing.");

            std::filesystem::path p (output_prefix);
            if (!is_dir(p.parent_path().string()))
                FATAL_ERROR("Output path prefix is not in a valid directory.");                 
        }
};


/* Function Declartions */
void parse_build_options(int argc, char** argv, PFPDocBuildOptions* opts);
void print_build_status_info(PFPDocBuildOptions* opts);

#endif /* End of PFP_DOC_H */