/*
 * File: pfp_doc.cpp
 * Description: Main file for building the document array profiles
 *              based on the prefix-free parse. This workflow is 
 *              based on the pfp_lcp.hpp developed by Massimiliano
 *              Rossi.
 *
 * Date: August 30th, 2022
 */

#include <pfp_doc.hpp> 
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <filesystem>


int is_file(std::string path) {
    /* Checks if the path is a valid file-path */
    std::ifstream test_file(path.data());
    if (test_file.fail()) {return 0;}
    
    test_file.close();
    return 1;
}

int is_dir(std::string path) {
    /* Checks if the directory is a valid path */
    return std::filesystem::exists(path);
}

void print_build_status_info(PFPDocBuildOptions* opts) {
    /* prints out the information being used in the current run */
    std::fprintf(stdout, "\nOverview of Parameters:\n");
    std::fprintf(stdout, "\tInput file-list: %s\n", opts->input_list.data());
    std::fprintf(stdout, "\tOutput ref path: %s\n", opts->output_ref.data());
    std::fprintf(stdout, "\tPFP window size: %d\n", opts->pfp_w);
}

void parse_build_options(int argc, char** argv, PFPDocBuildOptions* opts) {
    /* parses the arguments for the build sub-command, and returns a struct with arguments */
    int c = 0;
    while ((c = getopt(argc, argv, "hf:o:w:")) >= 0) {
        switch(c) {
            case 'h': pfpdoc_build_usage(); std::exit(1);
            case 'f': opts->input_list.assign(optarg); break;
            case 'o': opts->output_prefix.assign(optarg); break;
            case 'w': opts->pfp_w = std::atoi(optarg); break;
            default: pfpdoc_build_usage(); std::exit(1);
        }
    }
}

int build_main(int argc, char** argv) {
    /* main method for build the document profiles */
    if (argc == 1) return pfpdoc_build_usage();

    // grab the command-line options, and validate them
    PFPDocBuildOptions build_opts;
    parse_build_options(argc, argv, &build_opts);
    build_opts.validate();

    // determine output path for reference  
    build_opts.output_ref.assign(build_opts.output_prefix + ".fna");

    // print out information 
    print_build_status_info(&build_opts); 

    

    return 0;
}

int run_main(int argc, char** argv) {
    /* main method for querying the data-structure */
    std::cerr << "Error: querying is not implemented yet." << std::endl;
    return 0;
}

int pfpdoc_build_usage() {
    /* prints out the usage information for the build method */
    std::fprintf(stderr, "\npfp_doc build - builds the document array profiles using PFP.\n");
    std::fprintf(stderr, "Usage: pfp_doc build [options]\n\n");

    std::fprintf(stderr, "Options:\n");
    std::fprintf(stderr, "\t%-10sprints this usage message\n", "-h");
    std::fprintf(stderr, "\t%-10spath to a file-list of genomes to use\n", "-f [arg]");
    std::fprintf(stderr, "\t%-10soutput prefix path if using -f option\n", "-o [arg]");
    std::fprintf(stderr, "\t%-10swindow size used for pfp (default: 10)\n\n", "-w [INT]");

    return 0;
}

int pfpdoc_usage() {
    /* Prints the usage information for pfp_doc */
    std::fprintf(stderr, "\npfp_doc has different sub-commands to run:\n");
    std::fprintf(stderr, "Usage: pfp_doc <sub-command> [options]\n\n");

    std::fprintf(stderr, "Commands:\n");
    std::fprintf(stderr, "\tbuild\tbuilds the document profile data-structure\n");
    std::fprintf(stderr, "\trun\truns queries with respect to the document array structure\n\n");
    return 0;
}

int main(int argc, char** argv) {
    /* main method for pfp_doc */
    if (argc > 1) {
        if (std::strcmp(argv[1], "build") == 0) 
            return build_main(argc-1, argv+1);
        else if (std::strcmp(argv[1], "run") == 0)
            return run_main(argc-1, argv+1);
    }
    return pfpdoc_usage();
}