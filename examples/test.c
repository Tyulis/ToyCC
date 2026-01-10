int main(int argc, char** argv) {
    int a = 1 + 2 * (3 - 1);
    if (argc)
        return a;

    int b = a + 4 + argc;
    int c = a - b - argc;
    a = b + c;
    a += 2 - c*b;
    b = a - c;
    if (c)
        return a;
    else
        return b;
}
