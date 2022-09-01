// used for checking AST
// USAGE: clang-14 -Xclang -ast-dump -fsyntax-only tmp.c

int main() {
    int x;
    int y;

    if (x == 1) {
        return 1;
    } else if (y == 2) {
        return 2;
    } else if (x == y) {
        return 3;
    } else {
        return 4;
    }

    if (y / 2 == 3) {
        if (x == 2) {
            return -1;
        }
    }

    if (x)
        if (y)
            return -1;

    for (int i = 0; i < x; i += 2) {
        return 1;
    }

    for (y = 0; y <= x + 2; y--) {
        return 0;
    }

    for (int i = 0; ; i++);

    for (int i = 0; i <= 12; ++i);

    for (int i = 0; i <= 12; ++i) return 1;
}

