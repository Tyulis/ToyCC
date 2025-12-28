
int get_result(int argc) {
    const int x = 4;
    return 'a' + argc * x;
}

int main(int argc, char** argv) {
    return get_result(argc);
}
