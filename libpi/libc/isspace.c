int isspace(int c) {
    return (unsigned char)c == ' '  ||
           (unsigned char)c == '\t' ||
           (unsigned char)c == '\n' ||
           (unsigned char)c == '\v' ||
           (unsigned char)c == '\f' ||
           (unsigned char)c == '\r';
}
