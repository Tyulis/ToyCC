
int get_result(int argc) {
    {
        int a = 1;
        int b = 2;
        a += b;
    }
    const int x = 4;
    return 'a' + argc * x;
}

int main(int argc, char** argv) {
    return get_result(argc);
}
