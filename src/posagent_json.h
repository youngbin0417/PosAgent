#ifndef POSAGENT_JSON_H
#define POSAGENT_JSON_H

#include <stddef.h>

/* Internal, deliberately limited JSON Schema validator. */
int posagent_json_schema_supported(const char *schema);
int posagent_json_arguments_match(const char *schema, const char *arguments);

#endif
