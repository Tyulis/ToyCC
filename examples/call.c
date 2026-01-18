int func(int argc) {
    int a = argc + 1;
    return 2 * a;
}

int main(int argc, char** argv) {
    int b = 2 * argc;
    int c = func(argc);
    return c - b;
}
