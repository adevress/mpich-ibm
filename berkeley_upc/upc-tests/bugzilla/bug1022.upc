#include <string.h>
#include <stdio.h>

int main() {
  char s[100];
  const char *s2 = "hi there";
  char *x; 
  long long l;
  x = strchr(s2, 't');
  x = strrchr(s2, 't');
  x = strcpy(s, s2);
  x = strcat(s, s2);
  l = strcmp(s, s2);
  x = strncpy(s, s2, 4);
  x = strncat(s, s2, 4);
  l = strncmp(s, s2, 4);
  l = strlen(s);
  x = memchr(s2, 't',5);
  x = memcpy(s, s2, 10);
  x = memmove(s, s2, 10);
  x = memset(s, 0, 10);
  l = memcmp(s, s2, 10);

  return 0;
}
