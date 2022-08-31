/*
 * File: ref_builder.hpp
 * Description: Header for ref_builder.cpp that includes 
 *              definition of the RefBuilder class.
 * Date: August 31, 2022
 */

#ifndef REF_BUILD_H
#define REF_BUILD_H

#include <string>

class RefBuilder {
public:
    std::string input_file = "";
    std::string output_ref = "";
    
    RefBuilder(std::string input_data, std::string output_prefix);

}; // end of RefBuilder class


#endif /* end of REF_BUILD_H */