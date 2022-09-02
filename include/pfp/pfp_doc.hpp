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
#define STATUS_LOG(x, ...) do {std::fprintf(stdout, "[%s] ", x); std::fprintf(stdout, __VA_ARGS__ ); \
                               std::fprintf(stdout, " ... ");} while(0)
#define DONE_LOG(x) do {auto sec = std::chrono::duration<double>(x); \
                        std::fprintf(stdout, "done.  (%.3f sec)\n", sec.count());} while(0)

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
std::string execute_cmd(const char* cmd);

struct PFPDocBuildOptions {
    public:
        std::string input_list = "";
        std::string output_prefix = "";
        std::string output_ref = "";
        bool use_rcomp = false;
        size_t pfp_w = 10;
        size_t hash_mod = 100;
        size_t threads = 0;
        bool is_fasta = true;

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

struct HelperPrograms {
  /* Contains paths to run helper programs */
  std::string base_path = "";
  std::string parseNT_bin = "newscanNT.x";
  std::string parse_fasta_bin = "newscan.x";
  std::string parse_bin = "pscan.x";
  
public:
  void build_paths(std::string base) {
      /* Takes the base path, and combines it with names to generate executable paths */
      base_path.assign(base);
      parseNT_bin.assign(base_path + parseNT_bin);
      parse_fasta_bin.assign(base_path + parse_fasta_bin);
      parse_bin.assign(base_path + parse_bin);
  }

  void validate() const {
      /* Makes sure that each path for an executable is valid */
      bool invalid_path = !is_file(parseNT_bin) | !is_file(parse_fasta_bin) | !is_file(parse_bin);
      if (invalid_path) {FATAL_ERROR("One or more of helper program paths are invalid.");}
  }
};

/* Function Declartions involving structs */
void parse_build_options(int argc, char** argv, PFPDocBuildOptions* opts);
void print_build_status_info(PFPDocBuildOptions* opts);
void run_build_parse_cmd(PFPDocBuildOptions* build_opts, HelperPrograms* helper_bins);

#endif /* End of PFP_DOC_H */