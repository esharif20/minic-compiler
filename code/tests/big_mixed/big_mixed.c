int abs_int(int x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

float hyp_squared(float a, float b) {
    float s;
    s = a * a + b * b;
    return s;
}

int sum_from_1_to_n(int n) {
    int acc;
    int i;

    acc = 0;
    i = 1;
    while (i <= n) {
        acc = acc + i;
        i = i + 1;
    }
    return acc;
}

int big_mixed() {
    int score;
    float h;

    score = 0;

    if (abs_int(-3) == 3) {
        score = score + 1;
    }

    if (abs_int(5) == 5) {
        score = score + 1;
    }

    h = hyp_squared(3.0, 4.0);
    if (h == 25.0) {
        score = score + 1;
    }

    if (sum_from_1_to_n(5) == 15) {
        score = score + 1;
    }

    return score;   // expect 4
}
