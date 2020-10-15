/*
   Implements HTTP POST
*/

#ifndef http_h
#define http_h

/*
   http post to hostname:port/path with Auth header (optional)
*/
int http_post(char* hostname, int port, char* path, char* auth, char* body, int body_len, char* result, int result_len);

#endif
