#ifndef NETWORKING_H
#define NETWORKING_H

#include "ds.h"
#include <curl/curl.h>

typedef struct {
    s8 data;
    long code;
} DownloadResponse;

typedef struct {
    Arena *perm;
    s8 s;
} S8ArenaPair;

size_t curl_get_data(char *buf, size_t item_len, size_t item_num, void *data);
DownloadResponse download(Arena *perm, CURL *curl, s8 url);

#endif // NETWORKING_H

#ifdef NETWORKING_IMPL
#ifndef NETWORKING_IMPL_GUARD
#define NETWORKING_IMPL_GUARD

#define DS_IMPL
#include "ds.h"

size_t curl_get_data(char *buf, size_t item_len, size_t item_num, void *data) {
    size_t n = item_len * item_num;
    S8ArenaPair *into = (S8ArenaPair *)(data);

    char *r = new(into->perm, char, n);
    if (!into->s.buf) into->s.buf = (u8 *) r;
    memcpy(r, buf, n);
    into->s.len += n;

    return n;
}

DownloadResponse download(Arena *perm, CURL *curl, s8 url) {
    DownloadResponse ret = {0};

    url = s8_newcat(perm, url, s8("\0"));

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_get_data);

    S8ArenaPair data = { .perm = perm, };

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_URL, url.buf);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &data);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) warning("Failed to download resource: %s.", curl_easy_strerror(result));

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret.code);

    ret.data = data.s;

    return ret;
}

#endif // NETWORKING_IMPL_GUARD
#endif // NETWORKING_IMPL
