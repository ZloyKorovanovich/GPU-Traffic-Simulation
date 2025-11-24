#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"


typedef void (*WordOp) (const char* word, u32 word_length);

#define IS_SPACE(c) (c == ' ' || c == '\n' || c == '\t')

// calls an operation when ever finds every instance of a word in src string
u32 opOnAllWord(const char* src, const char* word, WordOp word_op) {
    u32 invocations = 0;

    const char* word_src = NULL;
    const char* src_ptr = src;
    const char* word_ptr = word;
    char input, search;
    u32 word_length = 0;
    u32 result;
    
_search_loop:
    input = *src_ptr;
    search = *word_ptr;

    if(search == '\0') {
        if(IS_SPACE(input) || input == '\0') {
            word_op(word_src, word_length);
            invocations++;
        }
        word_ptr = word;
        src_ptr++;
        word_length = 0;
        goto _search_loop;
    }
    if(input == '\0') {
        goto _exit;
    }

    result = (input == search);
    word_src = (result ? (word_src ? word_src : src_ptr) : NULL);
    
    word_length += result;
    word_ptr += result;
    src_ptr++;
    goto _search_loop;
_exit:
    return invocations;
}

// calls an operation when finds first instance of word in src string and quits
u32 opOnFirstWord(const char* src, const char* word, WordOp word_op) {
    u32 invocations = 0;

    const char* word_src = NULL;
    const char* src_ptr = src;
    const char* word_ptr = word;
    char input, search;
    u32 word_length = 0;
    u32 result;
    
_search_loop:
    input = *src_ptr;
    search = *word_ptr;

    if(search == '\0') {
        if(IS_SPACE(input) || input == '\0') {
            word_op(word_src, word_length);
            invocations++;
        }
        word_ptr = word;
        src_ptr++;
        word_length = 0;
        goto _exit;
    }
    if(input == '\0') {
        goto _exit;
    }

    result = (input == search);
    word_src = (result ? (word_src ? word_src : src_ptr) : NULL);
    
    word_length += result;
    word_ptr += result;
    src_ptr++;
    goto _search_loop;
_exit:
    return invocations;
}

typedef void (*PatternOp) (const char** args);

void wordOp(const char* word_begin, u32 word_length) {
    char word[256];
    memcpy(word, word_begin, word_length);
    printf("%s\n", word);
}


int main(int argc, char** argv) {
    opOnAllWord("Hello world, helloGet, hello no \n Way, hello world, \n hello", "hello", wordOp);
}
