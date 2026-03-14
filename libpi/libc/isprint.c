/* isprint(c) - true for printable ASCII (space ' ' through tilde '~') */
int isprint(int c) {
    return (unsigned char)c >= 0x20 && (unsigned char)c <= 0x7E;
}
