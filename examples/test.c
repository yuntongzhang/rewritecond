
int main() {
    int x = 0;
    int y = 5;

    // normal if
    if (x + 5 > 1) {
        return -1;
    } else {
        return 0;
    }

    // with else-if
    if (x == y) {
        return 1;
    } else if (x++ == 2) {
        return 2;
    } else if (y != 5) {
        return 3;
    } else {
        return 4;
    }

    // nested if, but ast looks like else-if
    if (x == y)
        if (x == 10)
            return -1;

    // nested if
    if (x == y) {
        x = 1;
        if (y != 2) {
            return -1;
        }
        return 1;
    }

    // if cond is a single var
    if (x) {
        return 1;
    }

    // if with non-compound body
    if (!(y == 22)) return 2;

    // normal while
    while (!(x == 1)) {
        return 1;
    }

    // nested while
    while (x == 1) {
        y += 2;
        while (y == 3) {
            x = 4;
        }
        return -1;
    }

    // while with non-compound body
    while (y <= 2) return 0;

    // normal for-loop
    for (int i = 0; i <= 5; i++) {
        return 0;
    }

    // for-loop where init is not declaration
    for (y = 0; y <= x + 2; y--) {
        return 0;
    }

    // SHOULDNT CHANGE - for-loop with empty condition
    for (int i = 0; ; i++) {
        return 0;
    }

    // SHOULDNT CHANGE - for-loop with empty condition and non-compound body
    for (int i = 0; ; i++);

    // for-loop with non-compound body (null statement)
    for (int i = 0; i <= 12; ++i);

    // for-loop with non-compound body (not null statement)
    for (int i = 0; i <= 12; ++i) return 1;
}
