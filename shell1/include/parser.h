#ifndef PARSER_H
#define PARSER_H

// Token types for tokenizer and parser
typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_BACKGROUND,
    TOKEN_SEMICOLON,
    TOKEN_INPUT_REDIRECT,
    TOKEN_OUTPUT_REDIRECT,
    TOKEN_OUTPUT_APPEND,
    TOKEN_INVALID
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    char *text;  // valid only if type == TOKEN_WORD
} Token;

// Parser state tracks tokens and current parse position
typedef struct {
    Token *tokens;
    int num_tokens;
    int pos;  // current index in tokens
} ParserState;

// Tokenizer API
int tokenize(const char *input, Token **tokens, int *num_tokens);
void free_tokens(Token *tokens, int num_tokens);

// Parser API: return 1 if syntax valid, else 0
int parse_shell_cmd(ParserState *ps);
int parse_cmd_group(ParserState *ps);
int parse_atomic(ParserState *ps);
int parse_input(ParserState *ps);
int parse_output(ParserState *ps);

#endif  // PARSER_H
