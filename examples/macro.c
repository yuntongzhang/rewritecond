#ifndef streq
#define	streq(a,b)	(strcmp((a),(b)) == 0)
#endif


#ifndef gtzero
#define gtzero(x)  ((x) > 0)
#endif

int main() {
    char *a = "1";
    char *b = "2";
    int c = -1;
    // usual macro
    if (streq(a, b))
        return 0;
    // macro as condition to avoid bracket
    if gtzero(c)
        return 1;
}
