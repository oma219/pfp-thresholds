/*
 * File: doc_queries.hpp
 * Description: Definition of doc_queries objects that performs queries
 *              on the document array profiles data-structure. It is based
 *              ms_pointers.hpp file that was written by Massimiliano Rossi.
 * Date: Sept. 10th, 2022
 */

#ifndef _DOC_QUERIES_H
#define _DOC_QUERIES_H

//#include <common.hpp>
//#include <malloc_count.h>



#include <sdsl/rmq_support.hpp>
#include <sdsl/int_vector.hpp>
#include <r_index.hpp>
#include <ms_rle_string.hpp>

template <class sparse_bv_type = ri::sparse_sd_vector,
          class rle_string_t = ms_rle_string_sd>
class doc_queries : ri::r_index<sparse_bv_type, rle_string_t>
{
    public:

    size_t num_docs = 0;
    std::vector<std::vector<size_t>> start_doc_profiles;
    std::vector<std::vector<size_t>> end_doc_profiles;

    typedef size_t size_type;

    doc_queries(std::string filename, bool rle = true): ri::r_index<sparse_bv_type, rle_string_t>()
    {
        std::string bwt_fname = filename + ".bwt";

        if(rle)
        {
            std::string bwt_heads_fname = bwt_fname + ".heads";
            std::ifstream ifs_heads(bwt_heads_fname);
            std::string bwt_len_fname = bwt_fname + ".len";
            std::ifstream ifs_len(bwt_len_fname);
            this->bwt = rle_string_t(ifs_heads, ifs_len);

            ifs_heads.seekg(0);
            ifs_len.seekg(0);
            this->build_F_(ifs_heads, ifs_len);

            ifs_heads.close();
            ifs_len.close();
        }
        else
        {
            std::ifstream ifs(bwt_fname);
            this->bwt = rle_string_t(ifs);

            ifs.seekg(0);
            this->build_F(ifs);

            ifs.close();
        }
        FORCE_LOG("build_doc_queries", "loaded the bwt of the input text");

        // gather some statistics on the BWT
        this->r = this->bwt.number_of_runs();
        ri::ulint n = this->bwt.size();
        size_t log_r = bitsize(uint64_t(this->r));
        size_t log_n = bitsize(uint64_t(this->bwt.size()));

        FORCE_LOG("build_doc_queries", "bwt statistics: n = %d, r = %d" , this->bwt.size(), this->r);
        
        /*
        for (size_t i = 0; i < this->bwt.size(); i++) {
            std::cout << " i = " << i << "   run # = " << this->bwt.run_of_position(i) << std::endl;
        }
        */
        
        // determine the number of documents to initialize doc profiles
        std::string tmp_filename = filename + ".sdap";

        struct stat filestat;
        FILE *fd;

        if ((fd = fopen(tmp_filename.c_str(), "r")) == nullptr)
            error("open() file " + tmp_filename + " failed");

        if (fstat(fileno(fd), &filestat) < 0)
            error("stat() file " + tmp_filename + " failed");
        ASSERT((filestat.st_size % 8 == 0), "invalid file size for document profiles.");

        num_docs = 0;
        if ((fread(&num_docs, sizeof(size_t), 1, fd)) != 1)
            error("fread() file " + tmp_filename + " failed"); 
        
        ASSERT((filestat.st_size == ((num_docs * this->r * sizeof(size_t)) + 8)), "invalid file size.");
        fclose(fd);
        
        // initialize the document array profiles
        start_doc_profiles.resize(this->r, std::vector<size_t>(num_docs, 0));
        end_doc_profiles.resize(this->r, std::vector<size_t>(num_docs, 0));

        // load the profiles for starts and ends
        read_doc_profiles(start_doc_profiles, filename + ".sdap", this->num_docs);
        read_doc_profiles(end_doc_profiles, filename + ".edap", this->num_docs);

        FORCE_LOG("build_doc_queries", "loaded the document array profiles");
    }

    static void read_doc_profiles(std::vector<std::vector<size_t>>& prof_matrix, std::string input_file, size_t num_docs) {
        /* loads a set of document array profiles into their respective matrix */

        // First, lets open the file and verify the size/# of docs are valid
        struct stat filestat; FILE *fd;

        if ((fd = fopen(input_file.c_str(), "r")) == nullptr)
            error("open() file " + input_file + " failed");

        if (fstat(fileno(fd), &filestat) < 0)
            error("stat() file " + input_file + " failed");
        ASSERT((filestat.st_size % 8 == 0), "invalid file size for document profiles.");

        size_t num_docs_found = 0;
        if ((fread(&num_docs_found, sizeof(size_t), 1, fd)) != 1)
            error("fread() file " + input_file + " failed"); 

        ASSERT((num_docs_found == num_docs), "mismatch in the number of documents.");
        ASSERT((filestat.st_size == ((num_docs * prof_matrix.size() * sizeof(size_t)) + 8)), "invalid file size.");

        // Secondly, go through the rest of file and fill in the profiles
        size_t curr_val = 0;
        for (size_t i = 0; i < prof_matrix.size(); i++) {
            for (size_t j = 0; j < num_docs; j++) {
                if ((fread(&curr_val, sizeof(size_t), 1, fd)) != 1)
                    error("fread() file " + input_file + " failed"); 
                prof_matrix[i][j] = curr_val;
            }
        }
        fclose(fd);
    }

    vector<ulint> build_F_(std::ifstream &heads, std::ifstream &lengths)
    {
        heads.clear();
        heads.seekg(0);
        lengths.clear();
        lengths.seekg(0);

        this->F = vector<ulint>(256, 0);
        int c;
        ulint i = 0;
        while ((c = heads.get()) != EOF)
        {
            size_t length;
            lengths.read((char *)&length, 5);
            if (c > TERMINATOR)
                this->F[c]+=length;
            else
            {
                this->F[TERMINATOR]+=length;
                this->terminator_position = i;
            }
            i++;
        }
        for (ulint i = 255; i > 0; --i)
            this->F[i] = this->F[i - 1];
        this->F[0] = 0;
        for (ulint i = 1; i < 256; ++i)
            this->F[i] += this->F[i - 1];
        return this->F;
    }

    ulint LF(ri::ulint i, ri::uchar c)
    {
        // //if character does not appear in the text, return empty pair
        // if ((c == 255 and this->F[c] == this->bwt_size()) || this->F[c] >= this->F[c + 1])
        //     return {1, 0};
        //number of c before the interval
        ri::ulint c_before = this->bwt.rank(i, c);
        // number of c inside the interval rn
        ri::ulint l = this->F[c] + c_before;
        return l;
    }

};

#endif /* end of include guard:_DOC_QUERIES_H */
