static inline int isxdigit(int c) {
    unsigned char u = (unsigned char)c;
    return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F');
}

int parse_byte_token(const char *piece, unsigned char *byte_val)
{
    // Check for the pattern: <0xHH>
    // Must be exactly 6 characters: < 0 x H H >
    if (piece[0] == '<' &&
        piece[1] == '0' &&
        piece[2] == 'x' &&
        isxdigit(piece[3]) &&
        isxdigit(piece[4]) &&
        piece[5] == '>' &&
        piece[6] == '\0')
    {
        // Convert the two hex digits manually
        unsigned char hi = piece[3];
        unsigned char lo = piece[4];

        // Convert hex char to value
        hi = (hi >= 'a') ? hi - 'a' + 10 : (hi >= 'A') ? hi - 'A' + 10
                                                       : hi - '0';
        lo = (lo >= 'a') ? lo - 'a' + 10 : (lo >= 'A') ? lo - 'A' + 10
                                                       : lo - '0';

        *byte_val = (hi << 4) | lo;
        return 1;
    }

    // no match
    return 0;
}