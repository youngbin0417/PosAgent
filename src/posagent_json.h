#ifndef POSAGENT_JSON_H
#define POSAGENT_JSON_H

#include <stddef.h>

/* Internal, deliberately limited JSON Schema validator. */
int posagent_json_schema_supported(const char *schema);
int posagent_json_arguments_match(const char *schema, const char *arguments);

/* Internal bounded JSON reader for provider response envelopes. */
typedef struct posagent_json_doc posagent_json_doc_t;
posagent_json_doc_t *posagent_json_parse(const char *text, size_t max_bytes);
void posagent_json_free(posagent_json_doc_t *doc);
int posagent_json_object_get(const posagent_json_doc_t *doc, int object, const char *key);
int posagent_json_array_count(const posagent_json_doc_t *doc, int array);
int posagent_json_array_item(const posagent_json_doc_t *doc, int array, int item);
int posagent_json_is_object(const posagent_json_doc_t *doc, int token);
int posagent_json_is_array(const posagent_json_doc_t *doc, int token);
int posagent_json_is_string(const posagent_json_doc_t *doc, int token);
int posagent_json_is_null(const posagent_json_doc_t *doc, int token);
int posagent_json_copy_string(const posagent_json_doc_t *doc, int token, char *out, size_t capacity);
int posagent_json_copy_raw(const posagent_json_doc_t *doc, int token, char *out, size_t capacity);

#endif
