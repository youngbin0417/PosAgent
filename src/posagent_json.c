#include "posagent_json.h"
#include "posagent.h"

#include <string.h>
#include <stdlib.h>

#define JSON_MAX_TOKENS 1024
#define JSON_MAX_DEPTH 32

typedef enum { JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL } json_kind_t;

typedef struct {
    json_kind_t kind;
    size_t start;
    size_t end;
    int next;
    int count;
} json_token_t;

struct posagent_json_doc {
    const char *text;
    size_t length;
    size_t position;
    json_token_t tokens[JSON_MAX_TOKENS];
    int count;
};
typedef struct posagent_json_doc json_doc_t;

static int unique_keys(const json_doc_t *doc, int value);

static void skip_space(json_doc_t *doc) {
    while (doc->position < doc->length &&
           (doc->text[doc->position] == ' ' || doc->text[doc->position] == '\t' ||
            doc->text[doc->position] == '\r' || doc->text[doc->position] == '\n')) {
        ++doc->position;
    }
}

static int hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int read_hex4(json_doc_t *doc, unsigned *value) {
    unsigned code = 0;
    if (doc->length - doc->position < 4) return 0;
    for (int i = 0; i < 4; ++i) {
        char c = doc->text[doc->position++];
        if (!hex_digit(c)) return 0;
        code = code * 16 + (unsigned)(c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10));
    }
    *value = code;
    return 1;
}

static int read_utf8_tail(json_doc_t *doc, unsigned char first) {
    int remaining;
    if (first >= 0xc2 && first <= 0xdf) remaining = 1;
    else if (first >= 0xe0 && first <= 0xef) remaining = 2;
    else if (first >= 0xf0 && first <= 0xf4) remaining = 3;
    else return 0;
    if (doc->length - doc->position < (size_t)remaining) return 0;
    unsigned char second = (unsigned char)doc->text[doc->position];
    if (second < 0x80 || second > 0xbf ||
        (first == 0xe0 && second < 0xa0) || (first == 0xed && second > 0x9f) ||
        (first == 0xf0 && second < 0x90) || (first == 0xf4 && second > 0x8f)) return 0;
    ++doc->position;
    for (int i = 1; i < remaining; ++i) {
        unsigned char next = (unsigned char)doc->text[doc->position++];
        if (next < 0x80 || next > 0xbf) return 0;
    }
    return 1;
}

static int parse_string(json_doc_t *doc) {
    ++doc->position; /* opening quote */
    while (doc->position < doc->length) {
        unsigned char c = (unsigned char)doc->text[doc->position++];
        if (c == '"') return 1;
        if (c < 0x20) return 0;
        if (c >= 0x80) {
            if (!read_utf8_tail(doc, c)) return 0;
            continue;
        }
        if (c != '\\') continue;
        if (doc->position >= doc->length) return 0;
        c = (unsigned char)doc->text[doc->position++];
        if (c == 'u') {
            unsigned high, low;
            if (!read_hex4(doc, &high)) return 0;
            if (high >= 0xd800 && high <= 0xdbff) {
                if (doc->length - doc->position < 2 || doc->text[doc->position] != '\\' || doc->text[doc->position + 1] != 'u') return 0;
                doc->position += 2;
                if (!read_hex4(doc, &low) || low < 0xdc00 || low > 0xdfff) return 0;
            } else if (high >= 0xdc00 && high <= 0xdfff) {
                return 0;
            }
        } else if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' && c != 'r' && c != 't') {
            return 0;
        }
    }
    return 0;
}

static int parse_number(json_doc_t *doc) {
    size_t p = doc->position;
    if (doc->text[p] == '-') ++p;
    if (p >= doc->length) return 0;
    if (doc->text[p] == '0') ++p;
    else {
        if (doc->text[p] < '1' || doc->text[p] > '9') return 0;
        do { ++p; } while (p < doc->length && doc->text[p] >= '0' && doc->text[p] <= '9');
    }
    if (p < doc->length && doc->text[p] == '.') {
        ++p;
        if (p >= doc->length || doc->text[p] < '0' || doc->text[p] > '9') return 0;
        do { ++p; } while (p < doc->length && doc->text[p] >= '0' && doc->text[p] <= '9');
    }
    if (p < doc->length && (doc->text[p] == 'e' || doc->text[p] == 'E')) {
        ++p;
        if (p < doc->length && (doc->text[p] == '+' || doc->text[p] == '-')) ++p;
        if (p >= doc->length || doc->text[p] < '0' || doc->text[p] > '9') return 0;
        do { ++p; } while (p < doc->length && doc->text[p] >= '0' && doc->text[p] <= '9');
    }
    doc->position = p;
    return 1;
}

static int parse_value(json_doc_t *doc, int depth) {
    skip_space(doc);
    if (depth > JSON_MAX_DEPTH || doc->position >= doc->length || doc->count >= JSON_MAX_TOKENS) return -1;
    int index = doc->count++;
    json_token_t *token = &doc->tokens[index];
    token->start = doc->position;
    token->count = 0;
    char c = doc->text[doc->position];
    if (c == '{' || c == '[') {
        token->kind = c == '{' ? JSON_OBJECT : JSON_ARRAY;
        char close = c == '{' ? '}' : ']';
        ++doc->position;
        skip_space(doc);
        if (doc->position < doc->length && doc->text[doc->position] == close) ++doc->position;
        else {
            for (;;) {
                if (c == '{') {
                    skip_space(doc);
                    if (doc->position >= doc->length || doc->text[doc->position] != '"' || parse_value(doc, depth + 1) < 0) return -1;
                    if (doc->tokens[doc->count - 1].kind != JSON_STRING) return -1;
                    skip_space(doc);
                    if (doc->position >= doc->length || doc->text[doc->position++] != ':') return -1;
                }
                if (parse_value(doc, depth + 1) < 0) return -1;
                ++token->count;
                skip_space(doc);
                if (doc->position >= doc->length) return -1;
                if (doc->text[doc->position] == close) { ++doc->position; break; }
                if (doc->text[doc->position++] != ',') return -1;
            }
        }
    } else if (c == '"') {
        token->kind = JSON_STRING;
        if (!parse_string(doc)) return -1;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        token->kind = JSON_NUMBER;
        if (!parse_number(doc)) return -1;
    } else {
        const char *literal = NULL;
        if (c == 't') { token->kind = JSON_TRUE; literal = "true"; }
        if (c == 'f') { token->kind = JSON_FALSE; literal = "false"; }
        if (c == 'n') { token->kind = JSON_NULL; literal = "null"; }
        if (literal == NULL || doc->length - doc->position < strlen(literal) || strncmp(doc->text + doc->position, literal, strlen(literal)) != 0) return -1;
        doc->position += strlen(literal);
    }
    token->end = doc->position;
    token->next = doc->count;
    return index;
}

static int parse_document(json_doc_t *doc, const char *text, size_t max_bytes) {
    size_t length = 0;
    if (text == NULL) return 0;
    while (length < max_bytes && text[length] != '\0') ++length;
    if (length == max_bytes) return 0;
    memset(doc, 0, sizeof(*doc));
    doc->text = text;
    doc->length = length;
    if (parse_value(doc, 0) != 0) return 0;
    skip_space(doc);
    return doc->position == doc->length && unique_keys(doc, 0);
}

/* Schema keys and property names are restricted to plain ASCII, unescaped text. */
static int plain_string(const json_doc_t *doc, int index, const char *value) {
    const json_token_t *token = &doc->tokens[index];
    size_t length = strlen(value);
    return token->kind == JSON_STRING && token->end - token->start == length + 2 &&
           memcmp(doc->text + token->start + 1, value, length) == 0;
}

static int schema_string_supported(const json_doc_t *doc, int index) {
    const json_token_t *token = &doc->tokens[index];
    if (token->kind != JSON_STRING) return 0;
    for (size_t i = token->start + 1; i + 1 < token->end; ++i) {
        unsigned char c = (unsigned char)doc->text[i];
        if (c < 0x20 || c > 0x7e || c == '"' || c == '\\') return 0;
    }
    return 1;
}

static int string_character(const json_doc_t *doc, size_t *position) {
    unsigned char c = (unsigned char)doc->text[(*position)++];
    if (c != '\\') return c;
    c = (unsigned char)doc->text[(*position)++];
    if (c == 'u') {
        unsigned code = 0;
        for (int i = 0; i < 4; ++i) {
            c = (unsigned char)doc->text[(*position)++];
            code = code * 16 + (unsigned)(c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10));
        }
        return code < 128 ? (int)code : -1;
    }
    if (c == 'b') return '\b';
    if (c == 'f') return '\f';
    if (c == 'n') return '\n';
    if (c == 'r') return '\r';
    if (c == 't') return '\t';
    return c;
}

static int same_string(const json_doc_t *a, int ai, const json_doc_t *b, int bi) {
    const json_token_t *at = &a->tokens[ai], *bt = &b->tokens[bi];
    size_t ap = at->start + 1, bp = bt->start + 1;
    size_t aend = at->end - 1, bend = bt->end - 1;
    if (at->kind != JSON_STRING || bt->kind != JSON_STRING) return 0;
    while (ap < aend && bp < bend) {
        int ac = string_character(a, &ap), bc = string_character(b, &bp);
        if (ac < 0 || ac != bc) return 0;
    }
    return ap == aend && bp == bend;
}

static int unique_keys(const json_doc_t *doc, int value) {
    json_kind_t kind = doc->tokens[value].kind;
    if (kind == JSON_ARRAY) {
        for (int item = value + 1; item < doc->tokens[value].next; item = doc->tokens[item].next)
            if (!unique_keys(doc, item)) return 0;
    } else if (kind == JSON_OBJECT) {
        for (int key = value + 1; key < doc->tokens[value].next; key = doc->tokens[key + 1].next) {
            size_t position = doc->tokens[key].start + 1;
            size_t end = doc->tokens[key].end - 1;
            while (position < end) {
                int character = string_character(doc, &position);
                if (character < 0 || character > 127) return 0;
            }
            for (int previous = value + 1; previous < key; previous = doc->tokens[previous + 1].next)
                if (same_string(doc, previous, doc, key)) return 0;
            if (!unique_keys(doc, key + 1)) return 0;
        }
    }
    return 1;
}

static int object_get(const json_doc_t *doc, int object, const char *name) {
    int key = object + 1;
    while (key < doc->tokens[object].next) {
        int value = key + 1;
        if (plain_string(doc, key, name)) return value;
        key = doc->tokens[value].next;
    }
    return -1;
}

static int schema_type(const json_doc_t *doc, int index) {
    static const char *names[] = { "object", "array", "string", "integer", "number", "boolean", "null" };
    for (int i = 0; i < 7; ++i) if (plain_string(doc, index, names[i])) return i;
    return -1;
}

static int schema_supported(const json_doc_t *doc, int schema, int depth) {
    if (depth > JSON_MAX_DEPTH || doc->tokens[schema].kind != JSON_OBJECT) return 0;
    int type = object_get(doc, schema, "type");
    if (type < 0 || schema_type(doc, type) < 0) return 0;
    int kind = schema_type(doc, type), key = schema + 1;
    int properties = -1, required = -1, additional = -1, items = -1;
    while (key < doc->tokens[schema].next) {
        int value = key + 1;
        if (plain_string(doc, key, "type")) { /* checked above */ }
        else if (plain_string(doc, key, "properties")) properties = value;
        else if (plain_string(doc, key, "required")) required = value;
        else if (plain_string(doc, key, "additionalProperties")) additional = value;
        else if (plain_string(doc, key, "items")) items = value;
        else return 0;
        key = doc->tokens[value].next;
    }
    if (kind != 0 && (properties >= 0 || required >= 0 || additional >= 0)) return 0;
    if (kind != 1 && items >= 0) return 0;
    if (kind == 1 && items < 0) return 0;
    if (additional >= 0 && doc->tokens[additional].kind != JSON_FALSE) return 0;
    if (properties >= 0) {
        if (doc->tokens[properties].kind != JSON_OBJECT) return 0;
        key = properties + 1;
        while (key < doc->tokens[properties].next) {
            int value = key + 1;
            if (!schema_string_supported(doc, key) || !schema_supported(doc, value, depth + 1)) return 0;
            key = doc->tokens[value].next;
        }
    }
    if (required >= 0) {
        if (doc->tokens[required].kind != JSON_ARRAY || properties < 0) return 0;
        for (int item = required + 1; item < doc->tokens[required].next; item = doc->tokens[item].next) {
            if (!schema_string_supported(doc, item)) return 0;
            int found = 0;
            for (key = properties + 1; key < doc->tokens[properties].next; key = doc->tokens[key + 1].next)
                if (same_string(doc, item, doc, key)) found = 1;
            if (!found) return 0;
        }
    }
    return items < 0 || schema_supported(doc, items, depth + 1);
}

static int number_is_integer(const json_doc_t *doc, int index) {
    const json_token_t *token = &doc->tokens[index];
    for (size_t i = token->start; i < token->end; ++i)
        if (doc->text[i] == '.' || doc->text[i] == 'e' || doc->text[i] == 'E') return 0;
    return 1;
}

static int match_schema(const json_doc_t *schema_doc, int schema, const json_doc_t *args, int value, int depth) {
    if (depth > JSON_MAX_DEPTH) return 0;
    int type = schema_type(schema_doc, object_get(schema_doc, schema, "type"));
    json_kind_t actual = args->tokens[value].kind;
    if (!((type == 0 && actual == JSON_OBJECT) || (type == 1 && actual == JSON_ARRAY) ||
          (type == 2 && actual == JSON_STRING) || (type == 3 && actual == JSON_NUMBER && number_is_integer(args, value)) ||
          (type == 4 && actual == JSON_NUMBER) || (type == 5 && (actual == JSON_TRUE || actual == JSON_FALSE)) ||
          (type == 6 && actual == JSON_NULL))) return 0;
    if (type == 1) {
        int items = object_get(schema_doc, schema, "items");
        for (int child = value + 1; child < args->tokens[value].next; child = args->tokens[child].next)
            if (!match_schema(schema_doc, items, args, child, depth + 1)) return 0;
    }
    if (type == 0) {
        int properties = object_get(schema_doc, schema, "properties");
        int required = object_get(schema_doc, schema, "required");
        int additional = object_get(schema_doc, schema, "additionalProperties");
        for (int key = value + 1; key < args->tokens[value].next; key = args->tokens[key + 1].next) {
            int property = -1;
            if (properties >= 0) {
                for (int candidate = properties + 1; candidate < schema_doc->tokens[properties].next; candidate = schema_doc->tokens[candidate + 1].next)
                    if (same_string(schema_doc, candidate, args, key)) { property = candidate + 1; break; }
            }
            if (property >= 0) {
                if (!match_schema(schema_doc, property, args, key + 1, depth + 1)) return 0;
            } else if (additional >= 0) return 0;
        }
        if (required >= 0) {
            for (int item = required + 1; item < schema_doc->tokens[required].next; item = schema_doc->tokens[item].next) {
                int found = 0;
                for (int key = value + 1; key < args->tokens[value].next; key = args->tokens[key + 1].next)
                    if (same_string(schema_doc, item, args, key)) found = 1;
                if (!found) return 0;
            }
        }
    }
    return 1;
}

int posagent_json_schema_supported(const char *schema) {
    json_doc_t doc;
    return parse_document(&doc, schema, POSAGENT_MAX_TOOL_JSON_SIZE) && schema_supported(&doc, 0, 0) && schema_type(&doc, object_get(&doc, 0, "type")) == 0;
}

int posagent_json_arguments_match(const char *schema, const char *arguments) {
    json_doc_t schema_doc, args_doc;
    return parse_document(&schema_doc, schema, POSAGENT_MAX_TOOL_JSON_SIZE) && parse_document(&args_doc, arguments, POSAGENT_MAX_TOOL_JSON_SIZE) &&
           schema_supported(&schema_doc, 0, 0) && schema_type(&schema_doc, object_get(&schema_doc, 0, "type")) == 0 &&
           match_schema(&schema_doc, 0, &args_doc, 0, 0);
}

posagent_json_doc_t *posagent_json_parse(const char *text, size_t max_bytes) {
    if (max_bytes == 0) return NULL;
    json_doc_t *doc = (json_doc_t *)malloc(sizeof(*doc));
    if (doc == NULL) return NULL;
    if (!parse_document(doc, text, max_bytes)) { free(doc); return NULL; }
    return doc;
}

void posagent_json_free(posagent_json_doc_t *doc) { free(doc); }

int posagent_json_object_get(const posagent_json_doc_t *doc, int object, const char *key) {
    if (doc == NULL || key == NULL || object < 0 || object >= doc->count || doc->tokens[object].kind != JSON_OBJECT) return -1;
    return object_get(doc, object, key);
}

int posagent_json_array_count(const posagent_json_doc_t *doc, int array) {
    return doc != NULL && array >= 0 && array < doc->count && doc->tokens[array].kind == JSON_ARRAY ? doc->tokens[array].count : -1;
}

int posagent_json_array_item(const posagent_json_doc_t *doc, int array, int item) {
    if (posagent_json_array_count(doc, array) <= item || item < 0) return -1;
    int token = array + 1;
    for (int i = 0; i < item; ++i) token = doc->tokens[token].next;
    return token;
}

int posagent_json_is_object(const posagent_json_doc_t *doc, int token) { return doc != NULL && token >= 0 && token < doc->count && doc->tokens[token].kind == JSON_OBJECT; }
int posagent_json_is_array(const posagent_json_doc_t *doc, int token) { return doc != NULL && token >= 0 && token < doc->count && doc->tokens[token].kind == JSON_ARRAY; }
int posagent_json_is_string(const posagent_json_doc_t *doc, int token) { return doc != NULL && token >= 0 && token < doc->count && doc->tokens[token].kind == JSON_STRING; }
int posagent_json_is_null(const posagent_json_doc_t *doc, int token) { return doc != NULL && token >= 0 && token < doc->count && doc->tokens[token].kind == JSON_NULL; }

static int append_byte(char *out, size_t capacity, size_t *used, unsigned char byte) {
    if (*used + 1 >= capacity) return 0;
    out[(*used)++] = (char)byte;
    return 1;
}

static int append_codepoint(char *out, size_t capacity, size_t *used, unsigned code) {
    if (code < 0x80) return append_byte(out, capacity, used, (unsigned char)code);
    if (code < 0x800) return append_byte(out, capacity, used, (unsigned char)(0xc0 | (code >> 6))) && append_byte(out, capacity, used, (unsigned char)(0x80 | (code & 63)));
    if (code < 0x10000) return append_byte(out, capacity, used, (unsigned char)(0xe0 | (code >> 12))) && append_byte(out, capacity, used, (unsigned char)(0x80 | ((code >> 6) & 63))) && append_byte(out, capacity, used, (unsigned char)(0x80 | (code & 63)));
    return append_byte(out, capacity, used, (unsigned char)(0xf0 | (code >> 18))) && append_byte(out, capacity, used, (unsigned char)(0x80 | ((code >> 12) & 63))) && append_byte(out, capacity, used, (unsigned char)(0x80 | ((code >> 6) & 63))) && append_byte(out, capacity, used, (unsigned char)(0x80 | (code & 63)));
}

static unsigned read_codepoint(const char *text, size_t *position) {
    unsigned code = 0;
    for (int i = 0; i < 4; ++i) {
        unsigned char c = (unsigned char)text[(*position)++];
        code = code * 16 + (unsigned)(c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10));
    }
    return code;
}

int posagent_json_copy_string(const posagent_json_doc_t *doc, int token, char *out, size_t capacity) {
    if (!posagent_json_is_string(doc, token) || out == NULL || capacity == 0) return 0;
    size_t used = 0, position = doc->tokens[token].start + 1, end = doc->tokens[token].end - 1;
    while (position < end) {
        unsigned char c = (unsigned char)doc->text[position++];
        if (c != '\\') { if (!append_byte(out, capacity, &used, c)) return 0; continue; }
        c = (unsigned char)doc->text[position++];
        if (c == 'u') {
            unsigned code = read_codepoint(doc->text, &position);
            if (code >= 0xd800 && code <= 0xdbff) {
                position += 2;
                code = 0x10000 + ((code - 0xd800) << 10) + read_codepoint(doc->text, &position) - 0xdc00;
            }
            if (code == 0 || !append_codepoint(out, capacity, &used, code)) return 0;
        } else {
            unsigned char decoded = c == 'b' ? '\b' : c == 'f' ? '\f' : c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c;
            if (decoded == 0 || !append_byte(out, capacity, &used, decoded)) return 0;
        }
    }
    out[used] = '\0';
    return 1;
}

int posagent_json_copy_raw(const posagent_json_doc_t *doc, int token, char *out, size_t capacity) {
    if (doc == NULL || token < 0 || token >= doc->count || out == NULL) return 0;
    size_t length = doc->tokens[token].end - doc->tokens[token].start;
    if (length >= capacity) return 0;
    memcpy(out, doc->text + doc->tokens[token].start, length);
    out[length] = '\0';
    return 1;
}
