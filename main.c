#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ============================================================
   BASE ⭐ COMPILER - All-in-One: Lexer + Parser + Codegen
   Compiles directly to x86-64 machine code
   ============================================================ */

#define MAX_TOKENS 10000
#define MAX_VARIABLES 1000
#define MAX_POOLS 500
#define MAX_FUNCTIONS 500
#define MAX_CODE 50000
#define MAX_LABELS 1000

/* Token types */
typedef enum {
    TOK_EOF,
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_FX,
    TOK_FUNCTION,
    TOK_ARROW,           /* -> */
    TOK_LBRACE,          /* { */
    TOK_RBRACE,          /* } */
    TOK_LBRACKET,        /* [ */
    TOK_RBRACKET,        /* ] */
    TOK_LPAREN,          /* ( */
    TOK_RPAREN,          /* ) */
    TOK_COMMA,           /* , */
    TOK_DOT,             /* . */
    TOK_COLON,           /* : */
    TOK_EQUALS,          /* = */
    TOK_QUOTE,           /* " */
    TOK_PLUS,            /* + */
    TOK_MINUS,           /* - */
    TOK_STAR,            /* * */
    TOK_SLASH,           /* / */
    TOK_PERCENT,         /* % */
    TOK_AND,             /* & */
    TOK_OR,              /* | */
    TOK_XOR,             /* ^ */
    TOK_NOT,             /* ~ */
    TOK_LT,              /* < */
    TOK_GT,              /* > */
    TOK_EQ,              /* == */
    TOK_NEQ,             /* != */
    TOK_LTE,             /* <= */
    TOK_GTE,             /* >= */
    TOK_REGISTER,        /* $0, $1, etc */
    TOK_LABEL,           /* label: */
    TOK_MEMPLUS,         /* MemPlus */
    TOK_MEMDEL,          /* MemDel */
    TOK_MEMEXIT,         /* MemExit */
    TOK_LOAD,            /* load */
    TOK_STORE,           /* store */
    TOK_ADD,             /* add */
    TOK_SUB,             /* sub */
    TOK_MUL,             /* mul */
    TOK_DIV,             /* div */
    TOK_MOD,             /* mod */
    TOK_CMP,             /* cmp */
    TOK_JMP,             /* jmp */
    TOK_JE,              /* je */
    TOK_JNE,             /* jne */
    TOK_JL,              /* jl */
    TOK_JG,              /* jg */
    TOK_JLE,             /* jle */
    TOK_JGE,             /* jge */
    TOK_CALL,            /* call */
    TOK_RETURN,          /* return */
    TOK_PUSH,            /* push */
    TOK_POP,             /* pop */
    TOK_B,               /* .B (bytes) */
    TOK_K,               /* .K (kilobytes) */
    TOK_M,               /* .M (megabytes) */
    TOK_G,               /* .G (gigabytes) */
    TOK_PERCENT_MOD,     /* .% (percentage) */
    TOK_PERCENT_ALLOC,   /* .%A (percentage of allocated) */
    TOK_DOT_S,           /* .s (print string) */
    TOK_HASH,            /* # (comment) */
    TOK_SEMILON,         /* ; */
    TOK_NEWLINE,
    TOK_P                /* p (pool prefix) */
} TokenType;

typedef struct {
    TokenType type;
    char value[256];
    int line;
} Token;

typedef struct {
    Token tokens[MAX_TOKENS];
    int count;
    int pos;
} TokenStream;

typedef struct {
    char name[256];
    int size;
    int allocated;  /* in bytes */
} Variable;

typedef struct {
    char name[256];
    int allocated;  /* in bytes */
    Variable vars[MAX_VARIABLES];
    int var_count;
} MemoryPool;

typedef struct {
    char name[256];
    int address;
    int size;
} Function;

typedef struct {
    char code[MAX_CODE];
    int pos;
} CodeBuffer;

/* Global state */
TokenStream tokens;
MemoryPool pools[MAX_POOLS];
int pool_count = 0;
Variable global_vars[MAX_VARIABLES];
int global_var_count = 0;
Function functions[MAX_FUNCTIONS];
int function_count = 0;
CodeBuffer code_buffer;
int label_counter = 0;
int total_allocated = 0;  /* For %A calculations */

/* ============================================================
   LEXER
   ============================================================ */

int is_identifier_start(char c) {
    return isalpha(c) || c == '_';
}

int is_identifier_char(char c) {
    return isalnum(c) || c == '_' || c == '-';
}

void skip_whitespace(const char* src, int* pos) {
    while (src[*pos] && (src[*pos] == ' ' || src[*pos] == '\t' || src[*pos] == '\r')) {
        (*pos)++;
    }
}

void skip_comment(const char* src, int* pos) {
    if (src[*pos] == '#') {
        while (src[*pos] && src[*pos] != '\n') {
            (*pos)++;
        }
    }
}

int read_string(const char* src, int* pos, char* buf) {
    int i = 0;
    (*pos)++;  /* skip opening quote */
    while (src[*pos] && src[*pos] != '"') {
        buf[i++] = src[*pos];
        (*pos)++;
    }
    if (src[*pos] == '"') (*pos)++;  /* skip closing quote */
    buf[i] = '\0';
    return i;
}

int read_number(const char* src, int* pos, char* buf) {
    int i = 0;
    while (src[*pos] && isdigit(src[*pos])) {
        buf[i++] = src[*pos];
        (*pos)++;
    }
    buf[i] = '\0';
    return i;
}

int read_identifier(const char* src, int* pos, char* buf) {
    int i = 0;
    while (src[*pos] && is_identifier_char(src[*pos])) {
        buf[i++] = src[*pos];
        (*pos)++;
    }
    buf[i] = '\0';
    return i;
}

TokenType keyword_to_token(const char* keyword) {
    if (strcmp(keyword, "fx") == 0) return TOK_FX;
    if (strcmp(keyword, "function") == 0) return TOK_FUNCTION;
    if (strcmp(keyword, "load") == 0) return TOK_LOAD;
    if (strcmp(keyword, "store") == 0) return TOK_STORE;
    if (strcmp(keyword, "add") == 0) return TOK_ADD;
    if (strcmp(keyword, "sub") == 0) return TOK_SUB;
    if (strcmp(keyword, "mul") == 0) return TOK_MUL;
    if (strcmp(keyword, "div") == 0) return TOK_DIV;
    if (strcmp(keyword, "mod") == 0) return TOK_MOD;
    if (strcmp(keyword, "cmp") == 0) return TOK_CMP;
    if (strcmp(keyword, "jmp") == 0) return TOK_JMP;
    if (strcmp(keyword, "je") == 0) return TOK_JE;
    if (strcmp(keyword, "jne") == 0) return TOK_JNE;
    if (strcmp(keyword, "jl") == 0) return TOK_JL;
    if (strcmp(keyword, "jg") == 0) return TOK_JG;
    if (strcmp(keyword, "jle") == 0) return TOK_JLE;
    if (strcmp(keyword, "jge") == 0) return TOK_JGE;
    if (strcmp(keyword, "call") == 0) return TOK_CALL;
    if (strcmp(keyword, "return") == 0) return TOK_RETURN;
    if (strcmp(keyword, "push") == 0) return TOK_PUSH;
    if (strcmp(keyword, "pop") == 0) return TOK_POP;
    if (strcmp(keyword, "MemPlus") == 0) return TOK_MEMPLUS;
    if (strcmp(keyword, "MemDel") == 0) return TOK_MEMDEL;
    if (strcmp(keyword, "MemExit") == 0) return TOK_MEMEXIT;
    if (strcmp(keyword, "p") == 0) return TOK_P;
    return TOK_IDENT;
}

void tokenize(const char* src) {
    int pos = 0;
    tokens.count = 0;
    int line = 1;

    while (src[pos] && tokens.count < MAX_TOKENS) {
        skip_whitespace(src, &pos);
        skip_comment(src, &pos);

        if (!src[pos]) break;

        Token tok;
        tok.line = line;
        tok.value[0] = '\0';

        /* Newline */
        if (src[pos] == '\n') {
            tok.type = TOK_NEWLINE;
            tokens.tokens[tokens.count++] = tok;
            pos++;
            line++;
            continue;
        }

        /* String */
        if (src[pos] == '"') {
            tok.type = TOK_STRING;
            read_string(src, &pos, tok.value);
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        /* Register ($0, $1, etc) */
        if (src[pos] == '$' && isdigit(src[pos + 1])) {
            tok.type = TOK_REGISTER;
            int i = 0;
            tok.value[i++] = src[pos++];
            while (src[pos] && isdigit(src[pos])) {
                tok.value[i++] = src[pos++];
            }
            tok.value[i] = '\0';
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        /* Numbers */
        if (isdigit(src[pos])) {
            tok.type = TOK_NUMBER;
            read_number(src, &pos, tok.value);
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        /* Identifiers and keywords */
        if (is_identifier_start(src[pos]) || src[pos] == '_') {
            char ident[256];
            read_identifier(src, &pos, ident);
            tok.type = keyword_to_token(ident);
            strcpy(tok.value, ident);
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        /* Two-character operators */
        if (src[pos] == '-' && src[pos + 1] == '>') {
            tok.type = TOK_ARROW;
            pos += 2;
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        if (src[pos] == '=' && src[pos + 1] == '=') {
            tok.type = TOK_EQ;
            pos += 2;
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        if (src[pos] == '!' && src[pos + 1] == '=') {
            tok.type = TOK_NEQ;
            pos += 2;
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        if (src[pos] == '<' && src[pos + 1] == '=') {
            tok.type = TOK_LTE;
            pos += 2;
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        if (src[pos] == '>' && src[pos + 1] == '=') {
            tok.type = TOK_GTE;
            pos += 2;
            tokens.tokens[tokens.count++] = tok;
            continue;
        }

        /* Single-character tokens */
        tok.value[0] = src[pos];
        tok.value[1] = '\0';

        switch (src[pos]) {
            case '{': tok.type = TOK_LBRACE; break;
            case '}': tok.type = TOK_RBRACE; break;
            case '[': tok.type = TOK_LBRACKET; break;
            case ']': tok.type = TOK_RBRACKET; break;
            case '(': tok.type = TOK_LPAREN; break;
            case ')': tok.type = TOK_RPAREN; break;
            case ',': tok.type = TOK_COMMA; break;
            case '.': tok.type = TOK_DOT; break;
            case ':': tok.type = TOK_COLON; break;
            case '=': tok.type = TOK_EQUALS; break;
            case '+': tok.type = TOK_PLUS; break;
            case '-': tok.type = TOK_MINUS; break;
            case '*': tok.type = TOK_STAR; break;
            case '/': tok.type = TOK_SLASH; break;
            case '%': tok.type = TOK_PERCENT; break;
            case '&': tok.type = TOK_AND; break;
            case '|': tok.type = TOK_OR; break;
            case '^': tok.type = TOK_XOR; break;
            case '~': tok.type = TOK_NOT; break;
            case '<': tok.type = TOK_LT; break;
            case '>': tok.type = TOK_GT; break;
            case ';': tok.type = TOK_SEMILON; break;
            case '#': tok.type = TOK_HASH; break;
            default:
                fprintf(stderr, "Unknown character: %c\n", src[pos]);
                pos++;
                continue;
        }

        pos++;
        tokens.tokens[tokens.count++] = tok;
    }

    tok.type = TOK_EOF;
    tok.value[0] = '\0';
    tokens.tokens[tokens.count++] = tok;
    tokens.pos = 0;
}

/* ============================================================
   PARSER HELPERS
   ============================================================ */

Token current_token() {
    if (tokens.pos < tokens.count) {
        return tokens.tokens[tokens.pos];
    }
    Token t;
    t.type = TOK_EOF;
    return t;
}

void advance() {
    if (tokens.pos < tokens.count) {
        tokens.pos++;
    }
}

int match(TokenType type) {
    if (current_token().type == type) {
        advance();
        return 1;
    }
    return 0;
}

int expect(TokenType type, const char* msg) {
    if (current_token().type != type) {
        fprintf(stderr, "ERROR: Expected %s but got %d\n", msg, current_token().type);
        return 0;
    }
    advance();
    return 1;
}

/* ============================================================
   CODE GENERATION
   ============================================================ */

void emit(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsprintf(code_buffer.code + code_buffer.pos, format, args);
    va_end(args);
    code_buffer.pos += len;
}

char* get_reg_name(const char* virt_reg) {
    /* Map virtual registers to x86-64 registers */
    static char names[][8] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9",
        "r10", "r11", "r12", "r13", "r14", "r15"
    };
    
    int reg_num = atoi(virt_reg + 1);
    if (reg_num < 14) {
        return names[reg_num];
    }
    
    /* For regs beyond 14, use stack (simplified) */
    static char fallback[32];
    sprintf(fallback, "[rbp-%d]", 8 + (reg_num - 14) * 8);
    return fallback;
}

int get_label() {
    return label_counter++;
}

/* ============================================================
   MAIN COMPILATION LOGIC
   ============================================================ */

void compile_instruction() {
    Token tok = current_token();

    switch (tok.type) {
        case TOK_LOAD: {
            advance();
            Token reg = current_token();
            expect(TOK_REGISTER, "register");
            expect(TOK_COMMA, ",");
            Token val = current_token();
            
            if (val.type == TOK_NUMBER) {
                advance();
                emit("  mov %s, %s\n", get_reg_name(reg.value), val.value);
            } else if (val.type == TOK_REGISTER) {
                advance();
                emit("  mov %s, %s\n", get_reg_name(reg.value), get_reg_name(val.value));
            } else if (val.type == TOK_LBRACKET) {
                advance();
                Token addr_reg = current_token();
                expect(TOK_REGISTER, "register");
                expect(TOK_RBRACKET, "]");
                emit("  mov %s, [%s]\n", get_reg_name(reg.value), get_reg_name(addr_reg.value));
            }
            break;
        }

        case TOK_STORE: {
            advance();
            expect(TOK_LBRACKET, "[");
            Token addr_reg = current_token();
            expect(TOK_REGISTER, "register");
            expect(TOK_RBRACKET, "]");
            expect(TOK_COMMA, ",");
            Token val_reg = current_token();
            expect(TOK_REGISTER, "register");
            
            emit("  mov [%s], %s\n", get_reg_name(addr_reg.value), get_reg_name(val_reg.value));
            break;
        }

        case TOK_ADD: {
            advance();
            Token dest = current_token();
            expect(TOK_REGISTER, "register");
            expect(TOK_COMMA, ",");
            Token src1 = current_token();
            expect(TOK_REGISTER, "register");
            expect(TOK_COMMA, ",");
            Token src2 = current_token();
            expect(TOK_REGISTER, "register");
            
            emit("  mov rax, %s\n", get_reg_name(src1.value));
            emit("  add rax, %s\n", get_reg_name(src2.value));
            emit("  mov %s, rax\n", get_reg_name(dest.value));
            break;
        }

        case TOK_RETURN: {
            advance();
            if (current_token().type == TOK_REGISTER) {
                Token reg = current_token();
                advance();
                emit("  mov rax, %s\n", get_reg_name(reg.value));
            }
            emit("  ret\n");
            break;
        }

        case TOK_CALL: {
            advance();
            if (current_token().type == TOK_FX || current_token().type == TOK_IDENT) {
                advance();
                expect(TOK_DOT, ".");
                Token func_name = current_token();
                expect(TOK_IDENT, "function name");
                emit("  call %s\n", func_name.value);
            }
            break;
        }

        case TOK_JMP: {
            advance();
            Token label = current_token();
            if (label.type == TOK_IDENT) {
                advance();
                emit("  jmp %s\n", label.value);
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown instruction: %d\n", tok.type);
            advance();
            break;
    }
}

void parse_function() {
    /* fx.functionname or function.functionname */
    int is_fx = match(TOK_FX);
    if (!is_fx) {
        match(TOK_FUNCTION);
    }

    expect(TOK_DOT, ".");
    Token func_name = current_token();
    expect(TOK_IDENT, "function name");

    expect(TOK_ARROW, "->");
    expect(TOK_LBRACE, "{");

    emit("%s:\n", func_name.value);

    while (current_token().type != TOK_RBRACE && current_token().type != TOK_EOF) {
        if (current_token().type == TOK_NEWLINE) {
            advance();
            continue;
        }
        compile_instruction();
    }

    expect(TOK_RBRACE, "}");
}

void compile_source(const char* src) {
    tokenize(src);
    
    code_buffer.pos = 0;
    emit("; BASE ⭐ COMPILED OUTPUT (x86-64)\n");
    emit("section .text\n");
    emit("global main\n\n");

    while (current_token().type != TOK_EOF) {
        if (current_token().type == TOK_NEWLINE) {
            advance();
            continue;
        }

        if (current_token().type == TOK_FX || current_token().type == TOK_FUNCTION) {
            parse_function();
        } else {
            advance();
        }
    }

    code_buffer.code[code_buffer.pos] = '\0';
}

/* ============================================================
   OUTPUT
   ============================================================ */

void output_to_file(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Failed to open output file: %s\n", filename);
        return;
    }

    fprintf(f, "%s", code_buffer.code);
    fclose(f);
    printf("Generated assembly: %s\n", filename);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <star_file>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Failed to open file: %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);

    printf("Compiling Base ⭐ code...\n");
    compile_source(src);

    output_to_file("output.asm");
    printf("Compilation complete!\n");

    free(src);
    return 0;
}
