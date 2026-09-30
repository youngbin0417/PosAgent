#include "posagent_chat.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>

static void clear_secret(char *buffer, size_t length) {
    volatile char *p = (volatile char *)buffer;
    while (length-- != 0) *p++ = 0;
}

static posagent_status_t transport_error(posagent_result_t *result, DWORD error) {
    posagent_status_t status = error == ERROR_INTERNET_TIMEOUT ? POSAGENT_ERR_TIMEOUT : POSAGENT_ERR_INTERNAL;
    result->status = status;
    result->code = (int32_t)error;
    snprintf(result->message, sizeof(result->message), "HTTPS transport failed (%lu)", (unsigned long)error);
    return status;
}

posagent_status_t posagent_chat_default_transport(const char *endpoint, const char *api_key, const char *body,
    uint32_t timeout_ms, char *response, size_t capacity, void *user_data, posagent_result_t *result) {
    (void)user_data;
    if (endpoint == NULL || body == NULL || response == NULL || capacity == 0 || result == NULL)
        return POSAGENT_ERR_INVALID_ARG;
    for (const char *p = api_key; p != NULL && *p != '\0'; ++p)
        if (*p == '\r' || *p == '\n') return transport_error(result, ERROR_INVALID_PARAMETER);

    char host[256] = {0}, path[2048] = {0}, extra[1024] = {0}, target[3072];
    URL_COMPONENTSA url = {0};
    url.dwStructSize = sizeof(url);
    url.lpszHostName = host; url.dwHostNameLength = sizeof(host);
    url.lpszUrlPath = path; url.dwUrlPathLength = sizeof(path);
    url.lpszExtraInfo = extra; url.dwExtraInfoLength = sizeof(extra);
    if (!InternetCrackUrlA(endpoint, 0, 0, &url) || url.nScheme != INTERNET_SCHEME_HTTPS ||
        host[0] == '\0' || strchr(endpoint, '@') != NULL ||
        snprintf(target, sizeof(target), "%s%s", path[0] != '\0' ? path : "/", extra) >= (int)sizeof(target))
        return transport_error(result, ERROR_INVALID_PARAMETER);

    HINTERNET session = NULL, connection = NULL, request = NULL;
    posagent_status_t status = POSAGENT_ERR_INTERNAL;
    session = InternetOpenA("PosAgent/0.1", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (session == NULL) return transport_error(result, GetLastError());
    DWORD timeout = timeout_ms;
    if (!InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout)) ||
        !InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout)) ||
        !InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout))) {
        status = transport_error(result, GetLastError()); goto cleanup;
    }
    connection = InternetConnectA(session, host, url.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (connection == NULL) { status = transport_error(result, GetLastError()); goto cleanup; }
    request = HttpOpenRequestA(connection, "POST", target, NULL, NULL, NULL,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD |
        INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_AUTO_REDIRECT |
        INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_AUTH, 0);
    if (request == NULL) { status = transport_error(result, GetLastError()); goto cleanup; }
    char headers[4096];
    int header_length;
    if (api_key != NULL && api_key[0] != '\0')
        header_length = snprintf(headers, sizeof(headers), "Content-Type: application/json\r\nAuthorization: Bearer %s\r\n", api_key);
    else header_length = snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");
    if (header_length < 0 || (size_t)header_length >= sizeof(headers) || strlen(body) > 0xffffffffUL) {
        clear_secret(headers, sizeof(headers));
        status = transport_error(result, ERROR_INVALID_PARAMETER); goto cleanup;
    }
    BOOL sent = HttpSendRequestA(request, headers, (DWORD)header_length, (void *)body, (DWORD)strlen(body));
    DWORD send_error = sent ? 0 : GetLastError();
    clear_secret(headers, sizeof(headers));
    if (!sent) {
        status = transport_error(result, send_error); goto cleanup;
    }
    DWORD http_status = 0, status_size = sizeof(http_status);
    if (!HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &http_status, &status_size, NULL)) {
        status = transport_error(result, GetLastError()); goto cleanup;
    }
    result->code = (int32_t)http_status;
    if (http_status < 200 || http_status >= 300) {
        status = POSAGENT_ERR_INTERNAL;
        result->status = status;
        snprintf(result->message, sizeof(result->message), "provider HTTP status %lu", (unsigned long)http_status);
        goto cleanup;
    }
    size_t used = 0;
    for (;;) {
        DWORD read = 0;
        char overflow_byte;
        DWORD available = used + 1 >= capacity ? 1 : (DWORD)(capacity - used - 1);
        char *destination = used + 1 >= capacity ? &overflow_byte : response + used;
        if (!InternetReadFile(request, destination, available, &read)) {
            status = transport_error(result, GetLastError()); goto cleanup;
        }
        if (read == 0) { status = POSAGENT_OK; break; }
        if (destination == &overflow_byte) { status = POSAGENT_ERR_BUFFER_TOO_SMALL; break; }
        used += read;
    }
    response[used] = '\0';
    if (status == POSAGENT_ERR_BUFFER_TOO_SMALL) {
        result->status = status;
        snprintf(result->message, sizeof(result->message), "provider response exceeds configured limit");
    }
cleanup:
    if (request != NULL) InternetCloseHandle(request);
    if (connection != NULL) InternetCloseHandle(connection);
    InternetCloseHandle(session);
    return status;
}

#else

posagent_status_t posagent_chat_default_transport(const char *endpoint, const char *api_key, const char *body,
    uint32_t timeout_ms, char *response, size_t capacity, void *user_data, posagent_result_t *result) {
    (void)endpoint; (void)api_key; (void)body; (void)timeout_ms;
    (void)response; (void)capacity; (void)user_data;
    if (result != NULL) {
        result->status = POSAGENT_ERR_INTERNAL;
        snprintf(result->message, sizeof(result->message), "default HTTPS transport is available on Windows only");
    }
    return POSAGENT_ERR_INTERNAL;
}

#endif
