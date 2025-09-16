#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//----------------- Tokenizer Implementation -----------------

static char *strndup_local(const char *s, size_t n) {
    char *res = malloc(n + 1);
    if (!res) return NULL;
    memcpy(res, s, n);
    res[n] = '\0';
    return res;
}

int tokenize(const char *input, Token **tokens_ptr, int *num_tokens_ptr) {
    const char *p = input;
    int size = 10;
    int count = 0;
    Token *tokens = malloc(size * sizeof(Token));
    if (!tokens) return -1;

    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        if (count >= size) {
            size *= 2;
            Token *new_tokens = realloc(tokens, size * sizeof(Token));
            if (!new_tokens) {
                free_tokens(tokens, count);
                return -1;
            }
            tokens = new_tokens;
        }

        Token token;
        token.text = NULL;

        switch (*p) {
            case '|':
                token.type = TOKEN_PIPE;
                token.text = NULL;
                p++;
                break;
            case '&':
                token.type = TOKEN_BACKGROUND;
                token.text = NULL;
                p++;
                break;
            case ';':
                token.type = TOKEN_SEMICOLON;
                token.text = NULL;
                p++;
                break;
            case '<':
                token.type = TOKEN_INPUT_REDIRECT;
                token.text = NULL;
                p++;
                break;
            case '>':
                p++;
                if (*p == '>') {
                    token.type = TOKEN_OUTPUT_APPEND;
                    token.text = NULL;
                    p++;
                } else {
                    token.type = TOKEN_OUTPUT_REDIRECT;
                    token.text = NULL;
                }
                break;
            default: {
                token.type = TOKEN_WORD;
                const char *start = p;
                // Match all chars until whitespace or token char
                while (*p && !isspace((unsigned char)*p) && *p != '|' && *p != '&' && *p != ';' &&
                       *p != '<' && *p != '>') {
                    p++;
                }
                token.text = strndup_local(start, p - start);
                if (!token.text) {
                    free_tokens(tokens, count);
                    return -1;
                }
                break;
            }
        }
        tokens[count++] = token;
    }

    *tokens_ptr = tokens;
    *num_tokens_ptr = count;
    return 0;
}

void free_tokens(Token *tokens, int num_tokens) {
    for (int i = 0; i < num_tokens; ++i) {
        if (tokens[i].type == TOKEN_WORD && tokens[i].text) {
            free(tokens[i].text);
        }
    }
    free(tokens);
}

//----------------- Parser Implementation -----------------

static int match_token(ParserState *ps, TokenType type) {
    if (ps->pos < ps->num_tokens && ps->tokens[ps->pos].type == type) {
        ps->pos++;
        return 1;
    }
    return 0;
}

static int match_word(ParserState *ps) {
    if (ps->pos < ps->num_tokens && ps->tokens[ps->pos].type == TOKEN_WORD) {
        ps->pos++;
        return 1;
    }
    return 0;
}

// input -> < name
int parse_input(ParserState *ps) {
    if (match_token(ps, TOKEN_INPUT_REDIRECT)) {
        return match_word(ps);
    }
    return 1;  // Optional, so success if not present
}

// output -> > name | >> name
int parse_output(ParserState *ps) {
    if (match_token(ps, TOKEN_OUTPUT_REDIRECT) || match_token(ps, TOKEN_OUTPUT_APPEND)) {
        return match_word(ps);
    }
    return 1;  // Optional, success if not present
}

// atomic -> name (name | input | output)*
int parse_atomic(ParserState *ps) {
    if (!match_word(ps)) return 0;  // Must start with a name

    while (ps->pos < ps->num_tokens) {
        TokenType t = ps->tokens[ps->pos].type;
        if (t == TOKEN_WORD) {
            match_word(ps);
        } else if (t == TOKEN_INPUT_REDIRECT) {
            if (!parse_input(ps)) return 0;
        } else if (t == TOKEN_OUTPUT_REDIRECT || t == TOKEN_OUTPUT_APPEND) {
            if (!parse_output(ps)) return 0;
        } else {
            break;
        }
    }
    return 1;
}

// cmd_group -> atomic (\| atomic)*
int parse_cmd_group(ParserState *ps) {
    if (!parse_atomic(ps)) return 0;
    while (match_token(ps, TOKEN_PIPE)) {
        if (!parse_atomic(ps)) return 0;
    }
    return 1;
}

// shell_cmd -> cmd_group ((& | ;) cmd_group)* &?
int parse_shell_cmd(ParserState *ps) {
    if (!parse_cmd_group(ps)) return 0;
    while (1) {
        if (match_token(ps, TOKEN_BACKGROUND) || match_token(ps, TOKEN_SEMICOLON)) {
            if (!parse_cmd_group(ps)) return 0;
        } else {
            break;
        }
    }
    // Optional trailing &
    match_token(ps, TOKEN_BACKGROUND);

    return ps->pos == ps->num_tokens;
}
