/*
 * File: pfp_lcp_doc.hpp
 * Description: Based heavily on Massmiliano Rossi's pfp_lcp.hpp code, this 
 *              code add the ability to build the document array profiles
 *              based on the prefix-free parse of a text.
 * Date: September 1st, 2022
 */

#ifndef _LCP_PFP_DOC_HH
#define _LCP_PFP_DOC_HH

#include <common.hpp>
#include <iostream>
#include <sdsl/rmq_support.hpp>
#include <sdsl/int_vector.hpp>
extern "C" {
#include <gsacak.h>
}

#include <pfp.hpp>
#include <ref_builder.hpp>
#include <deque>

// struct for LCP queue, along with method for printing (debugging)
typedef struct
{
    size_t run_num = 0;
    uint8_t bwt_ch = 0;
    size_t doc_num = 0;
    bool is_start = false;
    bool is_end = false;
    size_t lcp_with_prev_suffix = 0;
} queue_entry_t;

std::ostream& operator<<(std::ostream& os, const queue_entry_t& lcp_queue_entry) {
    return os << "[run #: " << lcp_queue_entry.run_num <<
                 ", bwt ch: " << lcp_queue_entry.bwt_ch <<
                 ", doc num: " << lcp_queue_entry.doc_num << 
                 ", is_start: " << lcp_queue_entry.is_start <<
                 ", is_end: " << lcp_queue_entry.is_end <<
                 ", lcp_with_prev: " << lcp_queue_entry.lcp_with_prev_suffix << "]";
}

class pfp_lcp{
public:

    pf_parsing& pf;
    std::vector<size_t> min_s; // Value of the minimum lcp_T in each run of BWT_T
    std::vector<size_t> pos_s; // Position of the minimum lcp_T in each run of BWT_T

    uint8_t head; // character of current BWT run
    size_t length = 0; // length of the current BWT run

    bool rle; // run-length encode the BWT

    pfp_lcp(pf_parsing &pfp_, std::string filename, RefBuilder* ref_build, bool rle_ = true) : 
                pf(pfp_),
                min_s(1, pf.n),
                pos_s(1,0),
                head(0),
                num_docs(ref_build->num_docs),
                ch_doc_counters(256, std::vector<size_t>(ref_build->num_docs, 0)),
                rle(rle_)
                // heads(1, 0)
    {
        // Opening output files
        std::string outfile = filename + std::string(".lcp");
        if ((lcp_file = fopen(outfile.c_str(), "w")) == nullptr)
            error("open() file " + outfile + " failed");

        outfile = filename + std::string(".ssa");
        if ((ssa_file = fopen(outfile.c_str(), "w")) == nullptr)
            error("open() file " + outfile + " failed");

        outfile = filename + std::string(".esa");
        if ((esa_file = fopen(outfile.c_str(), "w")) == nullptr)
            error("open() file " + outfile + " failed");

        if (rle) {
            outfile = filename + std::string(".bwt.heads");
            if ((bwt_file = fopen(outfile.c_str(), "w")) == nullptr)
                error("open() file " + outfile + " failed");

            outfile = filename + std::string(".bwt.len");
            if ((bwt_file_len = fopen(outfile.c_str(), "w")) == nullptr)
                error("open() file " + outfile + " failed");
        } else {
            outfile = filename + std::string(".bwt");
            if ((bwt_file = fopen(outfile.c_str(), "w")) == nullptr)
                error("open() file " + outfile + " failed");
        }

        outfile = filename + std::string(".sdap");
        if ((sdap_file = fopen(outfile.c_str(), "w")) == nullptr)
            error("open() file " + outfile + " failed");

        outfile = filename + std::string(".edap");
        if ((edap_file = fopen(outfile.c_str(), "w")) == nullptr)
            error("open() file " + outfile + " failed");

        assert(pf.dict.d[pf.dict.saD[0]] == EndOfDict);

        // variables for bwt/lcp/sa construction
        phrase_suffix_t curr;
        phrase_suffix_t prev;

        // variables for doc profile construction 
        uint8_t prev_bwt_ch = 0;
        size_t curr_run_num = 0;
        size_t pos = 0;
        std::vector<size_t> curr_da_profile (ref_build->num_docs, 0);
        std::vector<bool> docs_to_collect (ref_build->num_docs, false);

        // reserve some room for the lcp queue to avoid reallocation/copying
        //lcp_queue.reserve(100);
        //lcp_queue_profiles.reserve(100 * ref_build->num_docs);

        inc(curr);
        while (curr.i < pf.dict.saD.size())
        {
            // Make sure current suffix is a valid proper phrase suffix (at least w characters but not whole phrase)
            if(is_valid(curr)){

                // Compute the next character of the BWT of T
                std::vector<phrase_suffix_t> same_suffix(1, curr);
                phrase_suffix_t next = curr;

                // Go through suffix array of dictionary and store all phrase ids with same suffix
                while (inc(next) && (pf.dict.lcpD[next.i] >= curr.suffix_length))
                {
                    assert(next.suffix_length >= curr.suffix_length);
                    assert((pf.dict.b_d[next.sn] == 0 && next.suffix_length >= pf.w) || (next.suffix_length != curr.suffix_length));
                    if (next.suffix_length == curr.suffix_length)
                    {
                        same_suffix.push_back(next);
                    }
                }

                // Hard case: phrases with different BWT characters precediing them
                int_t lcp_suffix = compute_lcp_suffix(curr, prev);

                typedef std::pair<int_t *, std::pair<int_t *, uint8_t>> pq_t;

                // using lambda to compare elements.
                auto cmp = [](const pq_t &lhs, const pq_t &rhs) {
                    return *lhs.first > *rhs.first;
                };
                
                // Merge a list of occurrences of each phrase in the BWT of the parse
                std::priority_queue<pq_t, std::vector<pq_t>, decltype(cmp)> pq(cmp);
                for (auto s: same_suffix)
                {
                    size_t begin = pf.pars.select_ilist_s(s.phrase + 1);
                    size_t end = pf.pars.select_ilist_s(s.phrase + 2);
                    pq.push({&pf.pars.ilist[begin], {&pf.pars.ilist[end], s.bwt_char}});
                }

                size_t prev_occ;
                bool first = true;
                while (!pq.empty())
                {
                    auto curr_occ = pq.top();
                    pq.pop();

                    if (!first)
                    {
                        // Compute the minimum s_lcpP of the the current and previous occurrence of the phrase in BWT_P
                        lcp_suffix = curr.suffix_length + min_s_lcp_T(*curr_occ.first, prev_occ);
                    }
                    first = false;

                    // Update min_s
                    print_lcp(lcp_suffix, j);
                    update_ssa(curr, *curr_occ.first);
                    update_bwt(curr_occ.second.second, 1);
                    update_esa(curr, *curr_occ.first);

                    ssa = (pf.pos_T[*curr_occ.first] - curr.suffix_length) % (pf.n - pf.w + 1ULL);
                    esa = (pf.pos_T[*curr_occ.first] - curr.suffix_length) % (pf.n - pf.w + 1ULL);


                    // Start of the DA Profiles code ----------------------------------------------
                    //std::cout << curr_occ.second.second <<  " " << lcp_suffix << " " << ssa << " " << ref_build->doc_ends_rank(ssa) <<std::endl;

                    uint8_t curr_bwt_ch = curr_occ.second.second;
                    size_t lcp_i = lcp_suffix;
                    size_t sa_i = ssa;
                    size_t doc_i = ref_build->doc_ends_rank(ssa);

                    // Determine whether current suffix is a run boundary
                    bool is_start = (pos == 0 || curr_bwt_ch != prev_bwt_ch) ? 1 : 0;
                    bool is_end = (pos == ref_build->total_length-1); // only special case, common case is below

                    // Handle scenario where the previous suffix was a end of a run (and now we know 
                    // because we see the next character). So we need to reach into queue.
                    if (pos != 0 && prev_bwt_ch != curr_bwt_ch) 
                        lcp_queue.back().is_end = 1; 
                    
                    if (is_start) {curr_run_num++;}
                    size_t pos_of_LF_i = (sa_i > 0) ? (sa_i - 1) : (ref_build->total_length-1);
                    size_t doc_of_LF_i = ref_build->doc_ends_rank(pos_of_LF_i);

                    // Add the current suffix data to LCP queue 
                    queue_entry_t curr_entry = {curr_run_num-1, curr_bwt_ch, doc_of_LF_i, is_start, is_end, lcp_i};
                    lcp_queue.push_back(curr_entry);
                    ch_doc_counters[curr_bwt_ch][doc_of_LF_i] += 1;

                    size_t min_lcp = lcp_i; // this is lcp with previous suffix
                    bool passed_same_document = false;
                    std::fill(docs_to_collect.begin(), docs_to_collect.end(), false);
                    docs_to_collect[doc_of_LF_i] = true;

                    // Re-initialize doc profiles and max lcp with current document (itself)
                    std::fill(curr_da_profile.begin(), curr_da_profile.end(), 0);
                    curr_da_profile[doc_of_LF_i] = ref_build->total_length - pos_of_LF_i;

                    // lambda to check if we haven't found a certain document
                    auto all = [&](std::vector<bool> docs_collected) {
                            bool found_all = true;
                            for (auto elem: docs_collected)
                                found_all &= elem;
                            return found_all;
                    };

                    // get index before current suffix, use int for signedness
                    int queue_pos = lcp_queue.size() - 2; 
                    while (queue_pos >= 0 && (!all(docs_to_collect) || !passed_same_document)) 
                    {  
                        // Case 1: Cross same character and document pair
                        if (lcp_queue[queue_pos].bwt_ch == curr_bwt_ch && 
                            lcp_queue[queue_pos].doc_num == doc_of_LF_i) 
                        {
                                passed_same_document = true;
                        } 
                        // Case 2: Cross same character, but a different document
                        else if (lcp_queue[queue_pos].bwt_ch == curr_bwt_ch &&
                                 lcp_queue[queue_pos].doc_num != doc_of_LF_i) 
                        {
                            // Case 2a: Update the current DA profile we haven't seen this document yet
                            if (!docs_to_collect[lcp_queue[queue_pos].doc_num]) 
                            {
                                curr_da_profile[lcp_queue[queue_pos].doc_num] = min_lcp + 1;
                                docs_to_collect[lcp_queue[queue_pos].doc_num] = true;
                            }

                            // Case 2b: Update the predecessor DA profile if its run boundary
                            // and we haven't passed the <ch, doc> pair as current suffix
                            if (!passed_same_document && (lcp_queue[queue_pos].is_start or lcp_queue[queue_pos].is_end))
                            {
                                size_t start_pos = ref_build->num_docs * queue_pos;
                                size_t curr_max_lcp = lcp_queue_profiles[start_pos + doc_of_LF_i];
                                lcp_queue_profiles[start_pos + doc_of_LF_i] = max(curr_max_lcp, min_lcp+1);
                            }
                        }
                        
                        min_lcp = std::min(min_lcp, lcp_queue[queue_pos].lcp_with_prev_suffix);
                        queue_pos--;
                    }

                    // Add the current profile to the vector (should always be multiple of # of docs)
                    for (auto elem: curr_da_profile)
                        lcp_queue_profiles.push_back(elem);

                    assert(lcp_queue_profiles.size() % ref_build->num_docs == 0);
                    assert(lcp_queue_profiles.size() == (lcp_queue.size() * ref_build->num_docs));

                    // Try to trim the LCP queue and adjust the count matrix
                    size_t curr_pos = 0;
                    size_t records_to_remove = 0;
                    while (curr_pos < lcp_queue.size()) {
                        uint8_t curr_ch = lcp_queue[curr_pos].bwt_ch;
                        size_t curr_doc = lcp_queue[curr_pos].doc_num;
                        assert(ch_doc_counters[curr_ch][curr_doc] >= 1);

                        // Means we cannot remove it from LCP queue
                        if (ch_doc_counters[curr_ch][curr_doc] == 1)
                            break;
                        else    
                            records_to_remove++;
                        curr_pos++;
                    }

                    // Remove those top n elements, and update counts
                    update_lcp_queue(records_to_remove);

                    // End of the DA Profiles code ----------------------------------------------
                    // Except for some update statements below ...


                    // Update prevs
                    prev_occ = *curr_occ.first;
                    prev_bwt_ch = curr_bwt_ch;

                    // Update pq
                    curr_occ.first++;
                    if (curr_occ.first != curr_occ.second.first)
                        pq.push(curr_occ);

                    j += 1;
                    pos += 1;
                }

                prev = same_suffix.back();
                curr = next;
            }
            else {
                inc(curr);
            }
        }

        /* DEBUGGING PRINT STATEMENTS

        std::cout << "\n====== after loop " << std::endl;
        for (size_t i = 0; i < lcp_queue.size(); i++) {
            std::cout << lcp_queue[i] << std::endl;
        }

        std::cout << "\n====== after loop " << std::endl;
        size_t k = 0;
        while (k < lcp_queue_profiles.size()) {
            for (size_t l = 0; l < ref_build->num_docs; l++) {
                std::cout << lcp_queue_profiles[k] << " "; 
                k++;
            }
            std::cout << "\n";
        }
        */

        // print last BWT char and SA sample
        print_sa();
        print_bwt();
        print_doc_profiles();

        // Close output files
        fclose(ssa_file);
        fclose(esa_file);
        fclose(bwt_file);
        fclose(lcp_file);

        if (rle)
            fclose(bwt_file_len);
    }

private:
    typedef struct
    {
        size_t i = 0; // This should be safe since the first entry of sa is always the dollarsign used to compute the sa
        size_t phrase = 0;
        size_t suffix_length = 0;
        int_da sn = 0;
        uint8_t bwt_char = 0;
    } phrase_suffix_t;

    // Each entry in this will represent a row in BWM
    std::deque<queue_entry_t> lcp_queue;

    // The ith continguous group of num_doc integers is a profile for lcp_queue[i]
    std::deque<size_t> lcp_queue_profiles;

    // matrix of counters (alphabet size * number of documents) for the <ch, doc> pairs in lcp_queue
    std::vector<std::vector<size_t>> ch_doc_counters;// (256, std::vector<size_t>(10, 0));
    
    size_t j = 0;

    size_t ssa = 0;
    size_t esa = 0;
    size_t num_docs = 0;

    FILE *sdap_file; // start of document array profiles
    FILE *edap_file; // end of document array profiles
    FILE *lcp_file; // LCP array
    FILE *bwt_file; // BWT (run characters if using rle)
    FILE *bwt_file_len; // lengths file is using rle
    FILE *ssa_file; // start of suffix array sample
    FILE *esa_file; // end of suffix array sample

    inline void print_doc_profiles() {
        /* Go through the leftover lcp queue, and print all the profiles for run boundaries */
        for (size_t i = 0; i < lcp_queue.size(); i++) {
            bool is_start = lcp_queue[0].is_start;
            bool is_end = lcp_queue[0].is_end;

            // remove the DA profile, and print if it's a boundary
            for (size_t j = 0; j < num_docs; j++) {
                size_t prof_val = lcp_queue_profiles.front();
                lcp_queue_profiles.pop_front();

                if (is_start && fwrite(&prof_val, sizeof(prof_val), 1, sdap_file) != 1)
                    error("SA write error 1");
                if (is_end && fwrite(&prof_val, sizeof(prof_val), 1, edap_file) != 1)
                    error("SA write error 1");
            }
        }
    }

    inline void update_lcp_queue(size_t num_to_remove) {
        /* remove the first n records from the lcp queue and update count matrix */

        for (size_t i = 0; i < num_to_remove; i++) {
            uint8_t curr_ch = lcp_queue[0].bwt_ch;
            size_t curr_doc = lcp_queue[0].doc_num;  
            bool is_start = lcp_queue[0].is_start;
            bool is_end = lcp_queue[0].is_end;

            ch_doc_counters[curr_ch][curr_doc] -= 1;
            lcp_queue.pop_front();

            // remove the DA profile, and print if it's a boundary
            for (size_t j = 0; j < num_docs; j++) {
                size_t prof_val = lcp_queue_profiles.front();
                lcp_queue_profiles.pop_front();

                if (is_start && fwrite(&prof_val, sizeof(prof_val), 1, sdap_file) != 1)
                    error("SA write error 1");
                if (is_end && fwrite(&prof_val, sizeof(prof_val), 1, edap_file) != 1)
                    error("SA write error 1");
            }
        }
    }

    inline bool inc(phrase_suffix_t& s)
    {
        s.i++;
        if (s.i >= pf.dict.saD.size())
            return false;
        s.sn = pf.dict.saD[s.i];
        s.phrase = pf.dict.rank_b_d(s.sn);
        // s.phrase = pf.dict.daD[s.i] + 1; // + 1 because daD is 0-based
        assert(!is_valid(s) || (s.phrase > 0 && s.phrase < pf.pars.ilist.size()));
        s.suffix_length = pf.dict.select_b_d(pf.dict.rank_b_d(s.sn + 1) + 1) - s.sn - 1;
        if(is_valid(s))
            s.bwt_char = (s.sn == pf.w ? 0 : pf.dict.d[s.sn - 1]);
        return true;
    }

    inline bool is_valid(phrase_suffix_t& s)
    {
        // avoid the extra w # at the beginning of the text
        if (s.sn < pf.w)
            return false;
        // Check if the suffix has length at least w and is not the complete phrase.
        if (pf.dict.b_d[s.sn] != 0 || s.suffix_length < pf.w)
            return false;
        
        return true;
    }
    
    inline int_t min_s_lcp_T(size_t left, size_t right)
    {
        // assume left < right
        if (left > right)
            std::swap(left, right);

        assert(pf.s_lcp_T[pf.rmq_s_lcp_T(left + 1, right)] >= pf.w);

        return (pf.s_lcp_T[pf.rmq_s_lcp_T(left + 1, right)] - pf.w);
    }

    inline int_t compute_lcp_suffix(phrase_suffix_t& curr, phrase_suffix_t& prev)
    {
        int_t lcp_suffix = 0;

        if (j > 0)
        {
            // Compute phrase boundary lcp
            lcp_suffix = pf.dict.lcpD[curr.i];
            for (size_t k = prev.i + 1; k < curr.i; ++k)
            {
                lcp_suffix = std::min(lcp_suffix, pf.dict.lcpD[k]);
            }

            if (lcp_suffix >= curr.suffix_length && curr.suffix_length == prev.suffix_length)
            {
                // Compute the minimum s_lcpP of the phrases following the two phrases
                // we take the first occurrence of the phrase in BWT_P
                size_t left = pf.pars.ilist[pf.pars.select_ilist_s(curr.phrase + 1)]; //size_t left = first_P_BWT_P[phrase];
                // and the last occurrence of the previous phrase in BWT_P
                size_t right = pf.pars.ilist[pf.pars.select_ilist_s(prev.phrase + 2) - 1]; //last_P_BWT_P[prev_phrase];
                
                lcp_suffix += min_s_lcp_T(left,right);
            }
        }

        return lcp_suffix;
    }

    inline void print_lcp(int_t val, size_t pos)
    {
        size_t tmp_val = val;
        if (fwrite(&tmp_val, THRBYTES, 1, lcp_file) != 1)
            error("LCP write error 1");
    }

    // We can put here the check if we want to store the LCP or stream it out
    inline void new_min_s(int_t val, size_t pos)
    {
        min_s.push_back(val);
        pos_s.push_back(j);
    }

    inline void update_ssa(phrase_suffix_t &curr, size_t pos)
    {
        ssa = (pf.pos_T[pos] - curr.suffix_length) % (pf.n - pf.w + 1ULL); // + pf.w;
        assert(ssa < (pf.n - pf.w + 1ULL));
    }

    inline void update_esa(phrase_suffix_t &curr, size_t pos)
    {
        esa = (pf.pos_T[pos] - curr.suffix_length) % (pf.n - pf.w + 1ULL); // + pf.w;
        assert(esa < (pf.n - pf.w + 1ULL));
    }

    inline void print_sa()
    {
        if (j < (pf.n - pf.w + 1ULL))
        {
            size_t pos = j;
            if (fwrite(&pos, SSABYTES, 1, ssa_file) != 1)
                error("SA write error 1");
            if (fwrite(&ssa, SSABYTES, 1, ssa_file) != 1)
                error("SA write error 2");
        }

        if (j > 0)
        {
            size_t pos = j - 1;
            if (fwrite(&pos, SSABYTES, 1, esa_file) != 1)
                error("SA write error 1");
            if (fwrite(&esa, SSABYTES, 1, esa_file) != 1)
                error("SA write error 2");
        }
    }

    inline void print_bwt()
    {   
        if (length > 0)
        {
            if (rle) {
                // write the head character
                if (fputc(head, bwt_file) == EOF)
                    error("BWT write error 1");

                // write the length of that run
                if (fwrite(&length, BWTBYTES, 1, bwt_file_len) != 1)
                    error("BWT write error 2");
            } else {
                for (size_t i = 0; i < length; ++i)
                {
                    if (fputc(head, bwt_file) == EOF)
                        error("BWT write error 1");
                }
            }
        }
    }

    inline void update_bwt(uint8_t next_char, size_t length_)
    {
        if (head != next_char)
        {
            print_sa();
            print_bwt();

            head = next_char;

            length = 0;
        }
        length += length_;

    }




};

#endif /* end of include guard: _LCP_PFP_HH */
