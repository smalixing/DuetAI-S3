#ifndef __URL_PARSE_H__
#define __URL_PARSE_H__

int url_parse(const char *url, char *scheme, int max_scheme_len, char *host, int max_host_len, int *port, char *path, int max_path_len);

#endif
